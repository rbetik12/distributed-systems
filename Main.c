#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include "IOMisc.h"
#include "ipc.h"

struct IOInfo ioInfo;
local_id curProcessId;
const int8_t childProcessAmount = 3;
pid_t currentPID = 0;
pid_t parentPID = 0;

void SetupOtherProcessPipes(local_id curProcessId, struct ProcessInfo *process)
{
    for (int pipeIndex = 0; pipeIndex < ioInfo.processAmount; pipeIndex++)
    {
        //If we go to current process pipe we set it to read-only mode, so current process can receive messages from other processes.
        if (pipeIndex == curProcessId)
        {
            if (close(process->pipe[pipeIndex][1]))
            {
                perror("close");
            }
            process->pipe[pipeIndex][1] = 0;
        }
            // Otherwise, we close whole pipe, because it isn't connected to our process.
        else
        {
            if (close(process->pipe[pipeIndex][0]))
            {
                perror("Close");
            }
            if (close(process->pipe[pipeIndex][1]))
            {
                perror("Close");
            }

            process->pipe[pipeIndex][0] = 0;
            process->pipe[pipeIndex][1] = 0;
        }
    }
}

void SetupCurrentProcessPipes(local_id curProcessId, struct ProcessInfo *process)
{
    for (int8_t pipeIndex = 0; pipeIndex < ioInfo.processAmount; pipeIndex++)
    {
        // We close process self-related pipes
        if (pipeIndex == curProcessId)
        {
            if (close(process->pipe[pipeIndex][0]) == -1)
            {
                perror("Close");
            }
            if (close(process->pipe[pipeIndex][1]) == -1)
            {
                perror("Close");
            }
            process->pipe[pipeIndex][0] = 0;
            process->pipe[pipeIndex][1] = 0;
        } else
        {
            // We set other pipes to write-only mode. Current process will write to other processes from them.
            if (close(process->pipe[pipeIndex][0]) == -1)
            {
                perror("Close");
            }

            process->pipe[pipeIndex][0] = 0;
        }
    }
}

void SetupProcessPipes(local_id curProcessId, struct ProcessInfo *processInfo, bool isCurrent)
{
    if (isCurrent)
    {
        SetupCurrentProcessPipes(curProcessId, processInfo);
    } else
    {
        SetupOtherProcessPipes(curProcessId, processInfo);
    }
}

bool RunChildProcess()
{
    currentPID = getpid();
    for (int8_t processIndex = 0; processIndex < ioInfo.processAmount; processIndex++)
    {
        struct ProcessInfo *process = &ioInfo.process[processIndex];
        if (process->pid == 0)
        {
            curProcessId = processIndex;
            break;
        }
    }
    for (int8_t processIndex = 0; processIndex < ioInfo.processAmount; processIndex++)
    {
        struct ProcessInfo *process = &ioInfo.process[processIndex];
        if (process->pid == 0)
        {
            SetupProcessPipes(curProcessId, process, true);
        } else
        {
            SetupProcessPipes(curProcessId, process, false);
        }
    }

    if (curProcessId == 0)
    {
        Message message;
        InitMessage(&message);
        SendString(ioInfo, 1, "Hello from process 0!", &message);
        Log(Debug, "Process 0 sent message to process 1.\n", 0);
        Log(MessageInfo, NULL, 1, &message);
    } else if (curProcessId == 1)
    {
        Message message;
        receive(&ioInfo, 0, &message);
        Log(Debug, "Process 1 received message from process 0.\n", 0);
        Log(MessageInfo, NULL, 1, &message);
    }

    return true;
}

int main()
{
    parentPID = getpid();
    currentPID = parentPID;
    memset(&ioInfo, 0, sizeof(ioInfo));
    ioInfo.processAmount = childProcessAmount;

    for (int processIndex = 0; processIndex < ioInfo.processAmount; processIndex++)
    {
        ioInfo.process[processIndex].pid = -1;
        for (int childProcessPipeIndex = 0; childProcessPipeIndex < ioInfo.processAmount; childProcessPipeIndex++)
        {
            if (pipe(ioInfo.process[processIndex].pipe[childProcessPipeIndex]) == -1)
            {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }
    }

    for (int i = 0; i < ioInfo.processAmount; i++)
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

    for (int i = 0; i < ioInfo.processAmount; i++)
    {
        wait(&ioInfo.process[i].pid);
    }

    return 0;
}
