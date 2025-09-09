#include "kv_task.h"

#define INITIAL_BACKOFF_US 100;
#define MAX_BACKOFF_US 10000;

extern void Process_Message(connection_t *c);
extern int Process_Protocal(connection_t *c);
extern uint8_t Thread_Pool_Run();
extern uint8_t Thread_Pool_Init(int);
extern void smart_backoff_strategy();