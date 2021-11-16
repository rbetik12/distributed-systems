#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "Utils.h"
#include "ipc.h"
#include "pa2345.h"
#include "IPCWrapper.h"

IPCInfo ipcInfo;
local_id currentLocalID = 0;
pid_t currentPID = 0;
pid_t parentPID = 0;
bool shouldUseMutex = false;

timestamp_t get_lamport_time()
{
    return ipcInfo.currentLamportTime;
}

bool RunChildProcess()
{
    currentPID = getpid();

    // Mark parent process as non-zero process to determine it
    ipcInfo.process[0].pid = -1;
    InitIO(&currentLocalID, &ipcInfo);
    //Start
    Message message;
    InitMessage(&message, STARTED);
    WriteFormatStringToMessage(&message,
                               log_started_fmt,
                               5,
                               message.s_header.s_local_time,
                               currentLocalID,
                               currentPID,
                               parentPID,
                               0);
    Log(Event, message.s_payload, 0);

    SendMulticastWrapper(&ipcInfo, &message);
    ReceiveAll(&ipcInfo, currentLocalID);
    //Start end

    if (shouldUseMutex)
    {

    } else
    {
        for (int i = 0; i < currentLocalID * 5; i++)
        {
            char outBuf[256];
            snprintf(outBuf, 256, log_loop_operation_fmt, currentLocalID, i, currentLocalID * 5);
            print(outBuf);
        }
    }

    //Done
    InitMessage(&message, DONE);
    WriteFormatStringToMessage(&message,
                               log_done_fmt,
                               3,
                               message.s_header.s_local_time,
                               currentLocalID,
                               0);
    Log(Event, message.s_payload, 0);
    SendMulticastWrapper(&ipcInfo, &message);
    ReceiveAll(&ipcInfo, currentLocalID);
    //Done end

    ShutdownIO(&ipcInfo);

    return true;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        exit(EXIT_FAILURE);
    }
    InitLog();
    parentPID = getpid();
    currentPID = parentPID;
    memset(&ipcInfo, 0, sizeof(ipcInfo));
    int processAmountIndex = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mutexl") == 0) {
            shouldUseMutex = true;
        }
        if (strcmp(argv[i], "-x") == 0) {
            processAmountIndex = i + 1;
        }
    }
    ipcInfo.processAmount = strtol(argv[processAmountIndex], NULL, 10) + 1;

    InitIOParent(&ipcInfo);
    InitIONonBlocking(&ipcInfo);

    for (int i = 1; i < ipcInfo.processAmount; i++)
    {
        ipcInfo.process[i].pid = fork();
        switch (ipcInfo.process[i].pid)
        {
            case -1:
            {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            case 0:
            {
                bool result = RunChildProcess();
                exit(!result);
            }
            default:
            {
                continue;
            }
        }
    }

    ipcInfo.process[0].pid = 0;
    InitIO(&currentLocalID, &ipcInfo);

    //Receive started
    ReceiveAll(&ipcInfo, currentLocalID);

    // Receive done
    ReceiveAll(&ipcInfo, currentLocalID);

    for (int i = 0; i < ipcInfo.processAmount; i++)
    {
        wait(&ipcInfo.process[i].pid);
    }

    ShutdownIO(&ipcInfo);
    ShutdownLog();

    return 0;
}
