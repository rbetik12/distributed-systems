#include "IOMisc.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>

void WriteFormatString(Message *message, const char *format, int argsAmount, ...)
{
    va_list valist;
    va_start(valist, argsAmount);
    vsnprintf(message->s_payload, MAX_PAYLOAD_LEN, format, valist);
    message->s_header.s_payload_len = strlen(message->s_payload) + 1;
    va_end(valist);
}

void InitMessage(Message *message, MessageType type, timestamp_t (*GetTimePtr)(void))
{
    memset(message, 0, sizeof(Message));
    message->s_header.s_magic = MESSAGE_MAGIC;
    message->s_header.s_local_time = GetTimePtr();
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

void SetupProcessPipes(local_id curProcessId, struct ProcessInfo *processInfo, struct IOInfo *ioInfo, bool isCurrent)
{
    if (isCurrent)
    {
        SetupCurrentProcessPipes(curProcessId, processInfo, ioInfo->processAmount);
    } else
    {
        SetupOtherProcessPipes(curProcessId, processInfo, ioInfo->processAmount);
    }
}

void InitIO(local_id *currentProcessId, struct IOInfo *ioInfo)
{
    for (int8_t processIndex = 0; processIndex < ioInfo->processAmount; processIndex++)
    {
        struct ProcessInfo *process = &ioInfo->process[processIndex];
        if (process->pid == 0)
        {
            *currentProcessId = processIndex;
            break;
        }
    }
    for (int8_t processIndex = 0; processIndex < ioInfo->processAmount; processIndex++)
    {
        struct ProcessInfo *process = &ioInfo->process[processIndex];
        if (process->pid == 0)
        {
            SetupProcessPipes(*currentProcessId, process, ioInfo, true);
        } else
        {
            SetupProcessPipes(*currentProcessId, process, ioInfo, false);
        }
    }
}

void ShutdownIO(struct IOInfo *ioInfo)
{
    for (int8_t processIndex = 0; processIndex < ioInfo->processAmount; processIndex++)
    {
        struct ProcessInfo processInfo = ioInfo->process[processIndex];
        for (int8_t pipeIndex = 0; pipeIndex < ioInfo->processAmount; pipeIndex++)
        {
            CLOSE_PIPE(processInfo.pipe[pipeIndex], 0);
            CLOSE_PIPE(processInfo.pipe[pipeIndex], 1);
        }
    }
}

int InitIOParent(struct IOInfo *ioInfo)
{
    for (int processIndex = 0; processIndex < ioInfo->processAmount; processIndex++)
    {
        ioInfo->process[processIndex].pid = -1;
        for (int childProcessPipeIndex = 0; childProcessPipeIndex < ioInfo->processAmount; childProcessPipeIndex++)
        {
            if (childProcessPipeIndex != processIndex)
            {
                if (pipe(ioInfo->process[processIndex].pipe[childProcessPipeIndex]) == -1)
                {
                    perror("pipe");
                    return -1;
                }
            }
        }
    }

    return 0;
}

int ReceiveAll(struct IOInfo ioInfo, local_id currentLocalID)
{
    Message message;
    for (int8_t processIndex = 1; processIndex < ioInfo.processAmount; processIndex++)
    {
        if (processIndex == currentLocalID)
        {
            continue;
        }
        receive(&ioInfo, processIndex, &message);
        Log(Debug, "Process with local id: %d received message from process with local id: %d. Message: %s\n",
            3, currentLocalID, processIndex, message.s_payload);
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

int InitIONonBlocking(struct IOInfo *ioInfo)
{
    for (int processIndex = 0; processIndex < ioInfo->processAmount; processIndex++)
    {
        for (int childProcessPipeIndex = 0; childProcessPipeIndex < ioInfo->processAmount; childProcessPipeIndex++)
        {
            if (childProcessPipeIndex != processIndex)
            {
                if (SetPipeToNonBlocking(ioInfo->process[processIndex].pipe[childProcessPipeIndex][0]) == -1)
                {
                    return -1;
                }
                if (SetPipeToNonBlocking(ioInfo->process[processIndex].pipe[childProcessPipeIndex][1]) == -1)
                {
                    return -1;
                }
            }
        }
    }

    return 0;
}
