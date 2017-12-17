#pragma once
#include <kern/types.h>
#include <net/ether/protohdr.h>
#include <net/inet/protohdr.h>

#define MTU 1500
#define MSS (MTU-40)
#define IP_TTL 64
#define MAX_ARPTABLE 1024
#define ARBTBL_TIMEOUT_CLC 720 //10sec * 720 = 2hours
#define IPFRAG_TIMEOUT_CLC 6 //10sec * 6 = 1min

#define UDP_RECVQUEUE_LEN 32

#define TCP_TIMER_UNIT 200 //msec

#define TCP_RTT_INIT 3

#define TCP_PERSIST_WAIT_MAX 60000
#define TCP_RESEND_WAIT_MAX (60000*2)
#define TCP_FINWAIT_TIME (TCP_TIMER_UNIT*2)
#define TCP_TIMEWAIT_TIME (10000)
#define TCP_DELAYACK_TIME (TCP_TIMER_UNIT)

#define MAX_OPTLEN_IP 40

#define MAX_HDRLEN_ETHER (sizeof(struct ether_hdr))
#define MAX_HDRLEN_IP (MAX_HDRLEN_ETHER+sizeof(struct ip_hdr)+MAX_OPTLEN_IP)
#define MAX_HDRLEN_UDP (MAX_HDRLEN_ETHER+MAX_HDRLEN_IP+sizeof(struct udp_hdr))
#define MAX_HDRLEN_TCP (MAX_HDRLEN_ETHER+MAX_HDRLEN_IP+60)
