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


BalanceHistory balanceHistory;

void CheckHistory(timestamp_t (*GetTimePtr)(void))
{
    uint8_t historyIndex = GetTimePtr();

    if (balanceHistory.s_history[historyIndex].s_balance == 0)
    {
        balanceHistory.s_history[historyIndex].s_time = historyIndex;
        balanceHistory.s_history[historyIndex].s_balance = ipcInfo.process[currentLocalID].balance;
    }
}

void ProcessTransfer(Message *message, timestamp_t time)
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

        for (timestamp_t i = message->s_header.s_local_time - 1; i < time; i++)
        {
            balanceHistory.s_history[i].s_balance_pending_in += order.s_amount;
        }

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
    memset(&balanceHistory, 0, sizeof(balanceHistory));
    CheckHistory(get_lamport_time);
    balanceHistory.s_id = currentLocalID;
    //Start
    Message message;
    InitMessage(&message, STARTED);
    WriteFormatStringToMessage(&message, log_started_fmt, 5, message.s_header.s_local_time, currentLocalID, currentPID,
                               parentPID,
                               ipcInfo.process[currentLocalID].balance);
    Log(Event, message.s_payload, 0);

    SendMulticastWrapper(&ipcInfo, &message);
    ReceiveAll(&ipcInfo, currentLocalID);
    //Start end

    bool isRunning = true;
    while (isRunning)
    {
        CheckHistory(get_lamport_time);
        InitMessage(&message, STARTED);
        int retVal = receive_any(&ipcInfo, &message);
        if (retVal == EAGAIN)
        {
            continue;
        }

        const timestamp_t t = get_lamport_time();
        for (int i = balanceHistory.s_history_len; i < t; i++)
        {
            balanceHistory.s_history[i].s_balance = ipcInfo.process[currentLocalID].balance;
            balanceHistory.s_history[i].s_time = i;
        }

        balanceHistory.s_history_len = t;

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
                ProcessTransfer(&message, t);
                break;
            }
        }
    }

    balanceHistory.s_history[balanceHistory.s_history_len].s_time = balanceHistory.s_history_len;
    balanceHistory.s_history[balanceHistory.s_history_len].s_balance = ipcInfo.process[currentLocalID].balance;
    balanceHistory.s_history[balanceHistory.s_history_len].s_balance_pending_in = 0;
    balanceHistory.s_history_len += 1;

    InitMessage(&message, BALANCE_HISTORY);
    CopyToMessage(&message, &balanceHistory, sizeof(balanceHistory));
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
