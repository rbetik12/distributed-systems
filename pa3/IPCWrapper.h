#pragma once

int SendWrapper(void * self, local_id dst, Message * msg);

int SendMulticastWrapper(void * self, Message * msg);
