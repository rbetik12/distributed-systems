#include "IOMisc.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

int SendString(struct IOInfo ioInfo, local_id destination, const char *string, Message *message)
{
    WriteString(string, message);

    return send(&ioInfo, destination, message);
}

void WriteString(const char *string, Message *message)
{
    snprintf(message->s_payload, MAX_PAYLOAD_LEN, "%s", string);
    message->s_header.s_payload_len = strlen(string) + 1;
}

void WriteFormatString(Message *message, const char *format, int argsAmount, ...)
{
    va_list valist;
    va_start(valist, argsAmount);
    vsnprintf(message->s_payload, MAX_PAYLOAD_LEN, format, valist);
    message->s_header.s_payload_len = strlen(message->s_payload) + 1;
    va_end(valist);
}

void InitMessage(Message *message)
{
    memset(message, 0, sizeof(Message));
    message->s_header.s_magic = MESSAGE_MAGIC;
}

void SetupOtherProcessPipes(local_id curProcessId, struct ProcessInfo *process, int8_t processAmount)
{
    for (int pipeIndex = 0; pipeIndex < processAmount; pipeIndex++)
    {
        //If we go to current process pipe we set it to read-only mode, so current process can receive messages from other processes.
        if (pipeIndex == curProcessId)
        {
            CLOSE_PIPE(process->pipe[pipeIndex], 1)
        }
            // Otherwise, we close whole pipe, because it isn't connected to our process.
        else
        {
            CLOSE_PIPE(process->pipe[pipeIndex], 0)
            CLOSE_PIPE(process->pipe[pipeIndex], 1)
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
            CLOSE_PIPE(process->pipe[pipeIndex], 0)
            CLOSE_PIPE(process->pipe[pipeIndex], 1)
        } else
        {
            // We set other pipes to write-only mode. Current process will write to other processes from them.
            CLOSE_PIPE(process->pipe[pipeIndex], 0)
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
            CLOSE_PIPE(processInfo.pipe[pipeIndex], 0)
            CLOSE_PIPE(processInfo.pipe[pipeIndex], 1)
        }
    }
}
