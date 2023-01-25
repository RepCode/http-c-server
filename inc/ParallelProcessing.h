#ifndef PARALLEL_PROCESSING
#define PARALLEL_PROCESSING

#include <stdbool.h>

typedef enum CHILD_TYPE_ENUM {
     TRACKED = 0,
     UN_TRACKED
} CHILD_TYPE;

int Process_InitParallelHandling(int threadsMax);
int Process_StartChild(CHILD_TYPE type);
bool IsProcessRunning();
void Process_TearDown(void);
void Process_SignalTrackedChilds(int signal);
int Process_NewThread(void *(*start_routine) (void *), void *arg);
void Process_UpdateMaxThreads(int threadsMax);
void Process_ExitThread(void* returnVal);

#endif