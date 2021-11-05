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

timestamp_t get_lamport_time()
{
    return ipcInfo.currentLamportTime;
}

typedef struct BalanceHistoryWrapper
{
    BalanceHistory balanceHistory;
    uint8_t isSetByEvent[MAX_T + 1];
} BalanceHistoryWrapper;

BalanceHistoryWrapper balanceHistoryWrapper;

void CheckHistory(timestamp_t (*GetTimePtr)(void), int isEvent)
{
    uint8_t historyIndex = GetTimePtr();

    if (isEvent || balanceHistoryWrapper.balanceHistory.s_history[historyIndex].s_balance == 0)
    {
        balanceHistoryWrapper.balanceHistory.s_history[historyIndex].s_time = historyIndex;
        balanceHistoryWrapper.balanceHistory.s_history[historyIndex].s_balance = ipcInfo.process[currentLocalID].balance;
        balanceHistoryWrapper.balanceHistory.s_history_len += 1;
        balanceHistoryWrapper.isSetByEvent[historyIndex] = isEvent;
    }
}

void ProcessTransfer(Message *message)
{
    TransferOrder order;
    memcpy(&order, message->s_payload, sizeof(order));

    if (currentLocalID == order.s_src)
    {
        ipcInfo.process[currentLocalID].balance -= order.s_amount;
        SendWrapper(&ipcInfo, order.s_dst, message);
    } else if (currentLocalID == order.s_dst)
    {
        ipcInfo.process[currentLocalID].balance += order.s_amount;
        Message ackMessage;
        InitMessage(&ackMessage, ACK);

        SendWrapper(&ipcInfo, PARENT_ID, &ackMessage);
    }
}

bool RunChildProcess()
{
    currentPID = getpid();

    // Mark parent process as non-zero process to determine it
    ipcInfo.process[0].pid = -1;
    InitIO(&currentLocalID, &ipcInfo);
    memset(&balanceHistoryWrapper, 0, sizeof(balanceHistoryWrapper));
    balanceHistoryWrapper.balanceHistory.s_id = currentLocalID;
    balanceHistoryWrapper.balanceHistory.s_history_len = 1;
    balanceHistoryWrapper.balanceHistory.s_history[0].s_balance = ipcInfo.process[currentLocalID].balance;
    //Start
    Message message;
    InitMessage(&message, STARTED);
    WriteFormatStringToMessage(&message, log_started_fmt, 5, message.s_header.s_local_time, currentLocalID, currentPID,
                      parentPID,
                      ipcInfo.process[currentLocalID].balance);
    Log(Event, message.s_payload, 0);

    CheckHistory(get_lamport_time, 0);
    SendMulticastWrapper(&ipcInfo, &message);
    ReceiveAll(&ipcInfo, currentLocalID);
    //Start end

    bool isRunning = true;
    while (isRunning)
    {
        InitMessage(&message, STARTED);
        int retVal = receive_any(&ipcInfo, &message);
        if (retVal == EAGAIN)
        {
            continue;
        }
        switch (message.s_header.s_type)
        {
            case STOP:
            {
                Log(Debug, "Process with local id (%d) received STOP message\n", 1, currentLocalID);
                isRunning = false;
                break;
            }
            case TRANSFER:
            {
                ProcessTransfer(&message);
                break;
            }
        }
    }
    //Done
    InitMessage(&message, BALANCE_HISTORY);
    CopyToMessage(&message, &balanceHistoryWrapper.balanceHistory, sizeof(balanceHistoryWrapper.balanceHistory));
    SendWrapper(&ipcInfo, PARENT_ID, &message);

    InitMessage(&message, DONE);
    WriteFormatStringToMessage(&message, log_done_fmt, 3, message.s_header.s_local_time, currentLocalID,
                      ipcInfo.process[currentLocalID].balance);
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
    ipcInfo.processAmount = strtol(argv[2], NULL, 10) + 1;

    for (int i = 1; i < ipcInfo.processAmount; i++)
    {
        ipcInfo.process[i].balance = strtol(argv[2 + i], NULL, 10);
    }

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

    bank_robbery(&ipcInfo, ipcInfo.processAmount - 1);

    Message message;
    InitMessage(&message, STOP);
    SendMulticastWrapper(&ipcInfo, &message);

    AllHistory history;
    memset(&history, 0, sizeof(history));

    for (local_id id = 1; id < ipcInfo.processAmount; id++)
    {
        InitMessage(&message, BALANCE_HISTORY);
        while (receive(&ipcInfo, id, &message) == EAGAIN);

        history.s_history_len += 1;
        memcpy(&history.s_history[id - 1], message.s_payload, sizeof(BalanceHistory));
        history.s_history[id - 1].s_history_len = ipcInfo.processAmount;
    }

    print_history(&history);

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
