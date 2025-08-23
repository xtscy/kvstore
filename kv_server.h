



#ifndef __SERVER_H__
#define __SERVER_H__

#include <stdio.h>
#include <string.h>

#define BUFFER_LENGTH		1024

#define ENABLE_HTTP 		0
#define ENABLE_WEBSOCKET 	0
#define ENABLE_KVS 			1

#define NtyCo   1
#define Reactor 2
#define UseNet  1

// typedef int(*msg_handler)(char*, int, char*)
// msg_handler kv_handler;
extern int NtyCo_Entry(unsigned short);
extern int Reactor_Entry(unsigned short);
typedef int (*RCALLBACK)(int fd);
struct conn {
	int fd;

	char rbuffer[BUFFER_LENGTH];
	int rlength;

	char wbuffer[BUFFER_LENGTH];
	int wlength;

	RCALLBACK send_callback;

	union {
		RCALLBACK recv_callback;
		RCALLBACK accept_callback;
	} r_action;

	int status;
#if 0 // websocket
	char *payload;
	char mask[4];
#endif
};


#if UseNet == NtyCo
extern int kv_protocal(char *msg, int length, char *response);
#elif UseNet == Reactor
extern int kv_request(struct conn *c);
extern int kv_response(struct conn *c);
#endif




#if ENABLE_HTTP
int http_request(struct conn *c);
int http_response(struct conn *c);
#endif

#if ENABLE_WEBSOCKET
int ws_request(struct conn *c);
int ws_response(struct conn *c);
#endif


#endif


