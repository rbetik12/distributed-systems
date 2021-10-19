#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include "IOMisc.h"
#include "ipc.h"
#include "pa2345.h"

struct IOInfo ioInfo;
local_id currentLocalID = 0;
pid_t currentPID = 0;
pid_t parentPID = 0;

bool RunChildProcess()
{
    currentPID = getpid();

    // Mark parent process as non-zero process to determine it
    ioInfo.process[0].pid = -1;
    InitIO(&currentLocalID, &ioInfo);

    //Start
    Message message;
    InitMessage(&message);
    WriteFormatString(&message, log_started_fmt, 5, 0, currentLocalID, currentPID, parentPID,
                      ioInfo.process[currentLocalID].balance);
    Log(Event, message.s_payload, 0);
    send_multicast(&ioInfo, &message);
    InitMessage(&message);
    ReceiveAll(ioInfo, currentLocalID);
    //Start end

    //Done
    InitMessage(&message);
    WriteFormatString(&message, log_done_fmt, 3, get_physical_time(), currentLocalID,
                      ioInfo.process[currentLocalID].balance);
    Log(Event, message.s_payload, 0);
    message.s_header.s_type = DONE;
    send_multicast(&ioInfo, &message);
    ReceiveAll(ioInfo, currentLocalID);
    //Done end

    ShutdownIO(&ioInfo);

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
    memset(&ioInfo, 0, sizeof(ioInfo));
    ioInfo.processAmount = strtol(argv[2], NULL, 10) + 1;

    for (int i = 1; i < ioInfo.processAmount; i++)
    {
        ioInfo.process[i].balance = strtol(argv[2 + i], NULL, 10);
    }

    InitIOParent(&ioInfo);

    for (int i = 1; i < ioInfo.processAmount; i++)
    {
        ioInfo.process[i].pid = fork();
        switch (ioInfo.process[i].pid)
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

    ioInfo.process[0].pid = 0;
    InitIO(&currentLocalID, &ioInfo);

    //Receive started
    ReceiveAll(ioInfo, currentLocalID);

    get_physical_time();
    bank_robbery(&ioInfo, ioInfo.processAmount);

    // Receive done
    ReceiveAll(ioInfo, currentLocalID);

    for (int i = 0; i < ioInfo.processAmount; i++)
    {
        wait(&ioInfo.process[i].pid);
    }

    ShutdownIO(&ioInfo);
    ShutdownLog();

    return 0;
}
