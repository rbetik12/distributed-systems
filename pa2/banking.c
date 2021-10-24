#include "banking.h"
#include <errno.h>
#include "IOMisc.h"

void transfer(void * parentData, local_id src, local_id dst, balance_t amount)
{
    TransferOrder order;
    order.s_src = src;
    order.s_dst = dst;
    order.s_amount = amount;

    Message message;
    InitMessage(&message, TRANSFER, get_physical_time);
    CopyToMessage(&message, &order, sizeof(order));

    int status = send(parentData, src, &message);
    if (status == -1)
    {
        Log(Debug, "Can't send transfer from %d to %d!\n", 2, src, dst);
    }

    while((status = receive(parentData, dst, &message)) == EAGAIN);
    if (status == -1)
    {
        Log(Debug, "Can't receive ACK from %d!\n", 1, dst);
    }

    if (message.s_header.s_type == ACK)
    {
        Log(Debug, "Parent process received ACK from %d!\n", 1, dst);
    }
}
