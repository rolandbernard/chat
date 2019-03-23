// Copyright (c) 2019 Roland Bernard
#ifndef __NETIO_H__
#define __NETIO_H__

#include "types.h"

error_t net_sendmsg(int sock, const msgbuf_t* buffer);

error_t net_recvmsg(int sock, msgbuf_t* buffer);

#endif
