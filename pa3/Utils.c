#include "Utils.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

void WriteFormatStringToMessage(Message *message, const char *format, int argsAmount, ...)
{
    va_list valist;
    va_start(valist, argsAmount);
    vsnprintf(message->s_payload, MAX_PAYLOAD_LEN, format, valist);
    message->s_header.s_payload_len = strlen(message->s_payload) + 1;
    va_end(valist);
}

void InitMessage(Message *message, MessageType type)
{
    memset(message, 0, sizeof(Message));
    message->s_header.s_magic = MESSAGE_MAGIC;
    message->s_header.s_type = type;
}

void SetupOtherProcessPipes(local_id curProcessId, struct ProcessInfo *process, int8_t processAmount)
{
    for (int pipeIndex = 0; pipeIndex < processAmount; pipeIndex++)
    {
        //If we go to current process pipe we set it to read-only mode, so current process can receive messages from other processes.
        if (pipeIndex == curProcessId)
        {
            CLOSE_PIPE(process->pipe[pipeIndex], 1);
        }
            // Otherwise, we close whole pipe, because it isn't connected to our process.
        else
        {
            CLOSE_PIPE(process->pipe[pipeIndex], 0);
            CLOSE_PIPE(process->pipe[pipeIndex], 1);
        }
    }
}

void SetupCurrentProcessPipes(local_id curProcessId, struct ProcessInfo *process, int8_t processAmount)
{
    for (int8_t pipeIndex = 0; pipeIndex < processAmount; pipeIndex++)
    {
        // We close process self-related pipes
        if (pipeIndex == curProcessId)
        {
            CLOSE_PIPE(process->pipe[pipeIndex], 0);
            CLOSE_PIPE(process->pipe[pipeIndex], 1);
        } else
        {
            // We set other pipes to write-only mode. Current process will write to other processes from them.
            CLOSE_PIPE(process->pipe[pipeIndex], 0);
        }
    }
}

void SetupProcessPipes(local_id curProcessId, struct ProcessInfo *processInfo, IPCInfo *ipcInfo, bool isCurrent)
{
    if (isCurrent)
    {
        SetupCurrentProcessPipes(curProcessId, processInfo, ipcInfo->processAmount);
    } else
    {
        SetupOtherProcessPipes(curProcessId, processInfo, ipcInfo->processAmount);
    }
}

void InitIO(local_id *currentProcessId, IPCInfo *ipcInfo)
{
    for (int8_t processIndex = 0; processIndex < ipcInfo->processAmount; processIndex++)
    {
        struct ProcessInfo *process = &ipcInfo->process[processIndex];
        if (process->pid == 0)
        {
            *currentProcessId = processIndex;
            break;
        }
    }
    for (int8_t processIndex = 0; processIndex < ipcInfo->processAmount; processIndex++)
    {
        struct ProcessInfo *process = &ipcInfo->process[processIndex];
        if (process->pid == 0)
        {
            SetupProcessPipes(*currentProcessId, process, ipcInfo, true);
        } else
        {
            SetupProcessPipes(*currentProcessId, process, ipcInfo, false);
        }
    }
}

void ShutdownIO(IPCInfo *ipcInfo)
{
    for (int8_t processIndex = 0; processIndex < ipcInfo->processAmount; processIndex++)
    {
        struct ProcessInfo processInfo = ipcInfo->process[processIndex];
        for (int8_t pipeIndex = 0; pipeIndex < ipcInfo->processAmount; pipeIndex++)
        {
            CLOSE_PIPE(processInfo.pipe[pipeIndex], 0);
            CLOSE_PIPE(processInfo.pipe[pipeIndex], 1);
        }
    }
}

int InitIOParent(IPCInfo *ipcInfo)
{
    for (int processIndex = 0; processIndex < ipcInfo->processAmount; processIndex++)
    {
        ipcInfo->process[processIndex].pid = -1;
        for (int childProcessPipeIndex = 0; childProcessPipeIndex < ipcInfo->processAmount; childProcessPipeIndex++)
        {
            if (childProcessPipeIndex != processIndex)
            {
                if (pipe(ipcInfo->process[processIndex].pipe[childProcessPipeIndex]) == -1)
                {
                    perror("pipe");
                    return -1;
                }
            }
        }
    }

    return 0;
}

int ReceiveAll(IPCInfo *ipcInfo, local_id currentLocalID)
{
    Message message;
    for (int8_t processIndex = 1; processIndex < ipcInfo->processAmount; processIndex++)
    {
        if (processIndex == currentLocalID)
        {
            continue;
        }
        int retVal;
        while ((retVal = receive(ipcInfo, processIndex, &message) == EAGAIN));
        if (retVal)
        {
            return -1;
        }
    }

    return 0;
}

int SetPipeToNonBlocking(int pipeFd)
{
    int flags = fcntl(pipeFd, F_GETFL);
    if (flags == -1)
    {
        perror("Nonblock pipe");
        return -1;
    }
    if (fcntl(pipeFd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("Nonblock pipe");
        return -1;
    }

    return 0;
}

int InitIONonBlocking(IPCInfo *ipcInfo)
{
    for (int processIndex = 0; processIndex < ipcInfo->processAmount; processIndex++)
    {
        for (int childProcessPipeIndex = 0; childProcessPipeIndex < ipcInfo->processAmount; childProcessPipeIndex++)
        {
            if (childProcessPipeIndex != processIndex)
            {
                if (SetPipeToNonBlocking(ipcInfo->process[processIndex].pipe[childProcessPipeIndex][0]) == -1)
                {
                    return -1;
                }
                if (SetPipeToNonBlocking(ipcInfo->process[processIndex].pipe[childProcessPipeIndex][1]) == -1)
                {
                    return -1;
                }
            }
        }
    }

    return 0;
}

void CopyToMessage(Message *message, void* data, size_t dataSize)
{
    memcpy(message->s_payload, data, dataSize);
    message->s_header.s_payload_len = dataSize;
}
