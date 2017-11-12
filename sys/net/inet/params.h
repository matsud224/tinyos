#pragma once
#include <kern/types.h>

#define MTU 1500
#define MSS (MTU-40)
#define IP_TTL 64
#define MAX_ARPTABLE 1024
#define ARBTBL_TIMEOUT_CLC 720 //10sec * 720 = 2hours
#define IPFRAG_TIMEOUT_CLC 6 //10sec * 6 = 1min

#define UDP_RECVQUEUE_LEN 32

#define TCP_TIMER_UNIT 200 //最小の刻み(ミリ秒)
#define SECOND (TCP_TIMER_UNIT*5)
#define MINUTE (SECOND*60)
#define HOUR (MINUTE*60)

#define TCP_RTT_INIT 3

#define TCP_PERSIST_WAIT_MAX (MINUTE)
#define TCP_RESEND_WAIT_MAX (2*MINUTE)
#define TCP_FINWAIT_TIME (TCP_TIMER_UNIT*2)
#define TCP_TIMEWAIT_TIME (10*SECOND)
#define TCP_DELAYACK_TIME (TCP_TIMER_UNIT)

