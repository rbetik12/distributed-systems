#include "banking.h"

void transfer(void * parent_data, local_id src, local_id dst, balance_t amount)
{
    //TODO Implement
    int* data = (int*) parent_data;
    data += 1;
    src += 1;
    dst += 1;
    amount += 1;
}
