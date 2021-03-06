#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include "IOMisc.h"
#include "ipc.h"
#include "pa1.h"

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

    Message message;
    InitMessage(&message);
    WriteFormatString(&message, log_started_fmt, 3, currentLocalID, currentPID, parentPID);
    Log(Event, message.s_payload, 0);
    send_multicast(&ioInfo, &message);

    InitMessage(&message);

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

    InitMessage(&message);
    WriteFormatString(&message, log_done_fmt, 1, currentLocalID);
    Log(Event, message.s_payload, 0);
    message.s_header.s_type = DONE;
    send_multicast(&ioInfo, &message);

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

    for (int processIndex = 0; processIndex < ioInfo.processAmount; processIndex++)
    {
        ioInfo.process[processIndex].pid = -1;
        for (int childProcessPipeIndex = 0; childProcessPipeIndex < ioInfo.processAmount; childProcessPipeIndex++)
        {
            if (childProcessPipeIndex != processIndex)
            {
                if (pipe(ioInfo.process[processIndex].pipe[childProcessPipeIndex]) == -1)
                {
                    perror("pipe");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

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

    Message message;
    for (int8_t processIndex = 1; processIndex < ioInfo.processAmount; processIndex++)
    {
        if (processIndex == currentLocalID)
        {
            continue;
        }
        InitMessage(&message);
        receive(&ioInfo, processIndex, &message);
        Log(Debug, "Process with local id: %d received message from process with local id: %d. Message: %s\n",
            3, currentLocalID, processIndex, message.s_payload);
    }

    for (int8_t processIndex = 1; processIndex < ioInfo.processAmount; processIndex++)
    {
        if (processIndex == currentLocalID)
        {
            continue;
        }
        InitMessage(&message);
        receive(&ioInfo, processIndex, &message);
        Log(Debug, "Process with local id: %d received message from process with local id: %d. Message: %s\n",
            3, currentLocalID, processIndex, message.s_payload);
    }

    for (int i = 0; i < ioInfo.processAmount; i++)
    {
        wait(&ioInfo.process[i].pid);
    }

    ShutdownIO(&ioInfo);
    ShutdownLog();

    return 0;
}
