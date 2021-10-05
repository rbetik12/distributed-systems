#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include "IOMisc.h"
#include "ipc.h"

struct IOInfo ioInfo;
const int8_t childProcessAmount = 3;
local_id currentLocalID = 0;
pid_t currentPID = 0;
pid_t parentPID = 0;

bool RunChildProcess()
{
    currentPID = getpid();
    InitIO(&currentLocalID, &ioInfo);

    if (currentLocalID == 0)
    {
        Message message;
        InitMessage(&message);
        SendString(ioInfo, 1, "Hello from process 0!", &message);
        Log(Debug, "Process 0 sent message to process 1.\n", 0);
        Log(MessageInfo, NULL, 1, &message);
    } else if (currentLocalID == 1)
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
