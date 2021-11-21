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

int ReceiveMessage(IPCInfo *ipcInfo)
{
    Message message;
    InitMessage(&message, STARTED);
    int ret = receive_any(ipcInfo, &message);

    if (ret == EAGAIN)
    {
        return 0;
    } else if (ret < 0)
    {
        Log(Debug, "recieve %d\n", 1, ret);
        return -1;
    }

    local_id src;
    memcpy(&src, message.s_payload, sizeof(src));

    switch (message.s_header.s_type)
    {
        case CS_REQUEST:
        {
            switch (ipcInfo->state)
            {
                case FREE:
                {
                    Message reply;
                    InitMessage(&reply, CS_REPLY);
                    if (SendWrapper(ipcInfo, src, &reply) < 0)
                    {
                        return -1;
                    }
                    break;
                }
                case WAIT:
                {
                    if (ipcInfo->time > message.s_header.s_local_time
                    || (ipcInfo->time == message.s_header.s_local_time && currentLocalID > src))
                    {
                        Message reply;
                        InitMessage(&reply, CS_REPLY);
                        if (SendWrapper(ipcInfo, src, &reply) < 0)
                        {
                            return -1;
                        }
                    }
                    else
                    {
                        ipcInfo->deferredReply[src] = true;
                    }
                    break;
                }
                case BUSY:
                {
                    ipcInfo->deferredReply[src] = true;
                    break;
                }
            }
            break;
        }
        case CS_REPLY:
            ipcInfo->context.replies += 1;
            break;
        case DONE:
            ipcInfo->context.done += 1;
            break;
        default:
            Log(MessageInfo, NULL, 1, &message);
            return -1;
    }
    return 0;
}

timestamp_t get_lamport_time()
{
    return ipcInfo.currentLamportTime;
}

int request_cs(const void *self)
{
    IPCInfo *ipcInfo = (IPCInfo *) self;
    Message message;
    InitMessage(&message, CS_REQUEST);
    CopyToMessage(&message, &currentLocalID, sizeof(currentLocalID));

    int ret = SendMulticastWrapper(ipcInfo, &message);

    if (ret < 0)
    {
        Log(Debug, "send\n", 0);
        return -1;
    }

    ipcInfo->context.replies = 0;
    ipcInfo->state = WAIT;
    ipcInfo->time = get_lamport_time();

    while (ipcInfo->context.replies < ipcInfo->processAmount - 2)
    {
        if (ReceiveMessage(ipcInfo) < 0)
        {
            return -1;
        }
    }

    ipcInfo->state = BUSY;
    return 0;
}

int release_cs(const void *self)
{
    IPCInfo *ipcInfo = (IPCInfo *) self;

    for (int i = 1; i < ipcInfo->processAmount; i++) {
        if (ipcInfo->deferredReply[i])
        {
            Message reply;

            InitMessage(&reply, CS_REPLY);
            int status = SendWrapper(ipcInfo, i, &reply);
            if (status < 0)
            {
                return status;
            }

            ipcInfo->deferredReply[i] = false;
        }
    }

    ipcInfo->state = FREE;
    return 0;
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
        print(outBuf);

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

    while (ipcInfo.context.done < ipcInfo.processAmount - 2)
    {
        if (ReceiveMessage(&ipcInfo))
        {
            Log(Debug, "Error in proc %d\n", 1, currentLocalID);
            return -1;
        }
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
