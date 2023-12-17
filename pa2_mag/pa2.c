#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include "Utils.h"
#include "ipc.h"
#include "pa2345.h"
#include "IPCWrapper.h"

IPCInfo ipcInfo;
local_id currentLocalID = 0;
pid_t currentPID = 0;
pid_t parentPID = 0;
bool shouldUseMutex = false;
bool isFinished = false;

#define DEBUG_LOGGING

int checkMessages(void)
{
    Message message;
    int ret = receive_any(&ipcInfo, &message);

    if (ret == EAGAIN)
    {
        return 0;
    }
    else if (ret < 0)
    {
        Log(Debug, "recieve %d\n", 1, ret);
        return -1;
    }

    switch (message.s_header.s_type)
    {
        case CS_REQUEST:
        {
            ForkRequest request;
            memcpy(&request, &message.s_payload, sizeof(request));
#ifdef DEBUG_LOGGING
            Log(Debug, "%d: received request for fork %d from process %d\n", 3, currentLocalID, request.fork, request.senderId);
#endif

            ipcInfo.forks[request.senderId].requested = true;
            break;
        }
        case CS_REPLY:
        {
            local_id forkId;
            memcpy(&forkId, &message.s_payload, sizeof(local_id));

#ifdef DEBUG_LOGGING
            Log(Debug, "%d: received reply from %d\n", 2, currentLocalID, forkId);
#endif

            ipcInfo.forks[forkId].fork = true;
            ipcInfo.forks[forkId].dirty = false;
            ipcInfo.forks[forkId].requested = true;
            break;
        }
        case DONE:
            ipcInfo.context.done += 1;
            break;
    }

    return 0;
}

int sendMessages(void)
{
    for (int i = 1; i < ipcInfo.processAmount; i++)
    {
        if (i == currentLocalID)
        {
            continue;
        }

        struct ForkInfo fork = ipcInfo.forks[i];

        if (!fork.fork && ipcInfo.forks[i].requested)
        {
            ipcInfo.forks[i].requested = false;

            Message message;
            ForkRequest request;
            InitMessage(&message, CS_REQUEST);

            request.senderId = currentLocalID;
            request.fork = i;

            CopyToMessage(&message, &request, sizeof(request));
            SendWrapper(&ipcInfo, i, &message);
#ifdef DEBUG_LOGGING
            Log(Debug, "%d: sent request for fork to process %d\n", 2, currentLocalID, i);
#endif
        }

        if (fork.requested && fork.fork && (fork.dirty || isFinished))
        {
            Message reply;
            InitMessage(&reply, CS_REPLY);
            CopyToMessage(&reply, &currentLocalID, sizeof(currentLocalID));

            if (SendWrapper(&ipcInfo, i, &reply) < 0)
            {
                Log(Debug, "Process %d didn't send reply message to process %d! Error occured: %s\n", 3,
                    currentLocalID, i, strerror(errno));
                return -1;
            }

#ifdef DEBUG_LOGGING
            Log(Debug, "%d: Sending reply to %d\n", 2, currentLocalID, i);
#endif

            ipcInfo.forks[i].fork = false;
            ipcInfo.forks[i].dirty = false;
            ipcInfo.forks[i].requested = false;
        }
    }

    return 0;
}

timestamp_t get_lamport_time(void)
{
    return ipcInfo.currentLamportTime;
}

int request_cs(const void *self)
{
    while (true)
    {
        bool allClean = true;
        for (int i = 1; i < ipcInfo.processAmount; i++)
        {
            if (i == currentLocalID)
            {
                continue;
            }

            struct ForkInfo fork = ipcInfo.forks[i];

            if (!fork.fork || fork.dirty)
            {
                allClean = false;
                break;
            }
        }

        if (allClean)
        {
            return 0;
        }

        checkMessages();
        sendMessages();
    }

    return 0;
}

int release_cs(const void *self)
{
    for (int i = 1; i < ipcInfo.processAmount; i++)
    {
        if (i == currentLocalID)
        {
            continue;
        }
        ipcInfo.forks[i].dirty = true;
    }

    return 0;
}

bool RunChildProcess(void)
{
    currentPID = getpid();

    // Mark parent process as non-zero process to determine it
    ipcInfo.process[0].pid = -1;
    InitIO(&currentLocalID, &ipcInfo);

    Log(Debug, "PID: %d Proccess num: %d\n", 2, currentPID, currentLocalID);

    // Init forks

    for (int i = 1; i < ipcInfo.processAmount; i++)
    {
        if (i == currentLocalID)
        {
            continue;
        }

        if (i > currentLocalID)
        {
            ipcInfo.forks[i].dirty = true;
            ipcInfo.forks[i].fork = true;
            ipcInfo.forks[i].requested = false;
        }

        if (i < currentLocalID)
        {
            ipcInfo.forks[i].requested = true;
        }
    }

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

    for (int i = 0; i < currentLocalID * 5; i++)
    {
        ipcInfo.currentIteration = i;
        if (shouldUseMutex)
        {
            if (request_cs(&ipcInfo) < 0)
            {
                Log(Debug, "Request failed at %d step in process %d\n", 2, i + 1, currentLocalID);
                return false;
            }
        }

        char outBuf[256];
        snprintf(outBuf, 256, log_loop_operation_fmt, currentLocalID, i + 1, currentLocalID * 5);
//        print(outBuf);
        printf("%s\n", outBuf);

        if (shouldUseMutex)
        {
            if (release_cs(&ipcInfo) < 0)
            {
                Log(Debug, "Release failed at %d step in process %d\n", 2, i + 1, currentLocalID);
                return false;
            }
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

    Log(Debug, "Proc done msg: %d\n", 1, ipcInfo.context.done + 1);

    isFinished = true;

    while (ipcInfo.context.done < ipcInfo.processAmount - 2)
    {
        if (checkMessages())
        {
            Log(Debug, "Error in proc %d\n", 1, currentLocalID);
            return -1;
        }
        sendMessages();
    }
    //Done end
    Log(Debug, "Shutdown in proc %d\n", 1, currentLocalID);
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

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--mutexl") == 0)
        {
            shouldUseMutex = true;
        }
        if (strcmp(argv[i], "-p") == 0)
        {
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
