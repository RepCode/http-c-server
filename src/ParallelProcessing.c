#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include<pthread.h>

#include "AppConfig.h"
#include "ParallelProcessing.h"

static volatile bool ProcessRunning = true;

static int ThreadsMax;
static int ActiveThreads;
pthread_mutex_t ActiveThreadsLock;

/*
 * Function:  IsProcessRunning
 * --------------------
 *  retorna el valor de ProcessRunning, para que el proceso pueda saber si fue indicado que se detenga.
 *
 *  returns: booleano indicando si fue indicado el proceso deberia terminar.
 */
bool IsProcessRunning(){
    return ProcessRunning;
}

typedef struct ChildProcessStruct {
    int pid;
    struct ChildProcessStruct* previous;
} ChildProcess;

static ChildProcess* TrackedChildsEnd = NULL;

/*
 * Function:  ExitHandler
 * --------------------
 *  Signal Handler para detener el proceso. Solo cambia el valor de la variable global ProcessRunning, la cual
 *  los procesos usan para ver si deben hacer exit.
 *
 *  returns: void
 */
static void ExitHandler(void) {
    ProcessRunning = false;
}

/*
 * Function:  SigChildHandler
 * --------------------
 *  Handler de procesos hijo
 *
 *  returns: void
 */
#if !IS_OVER_POSIX2001
static void SigChildHandler(void) {
    int savedErrno = errno;
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
    errno = savedErrno;
}
#endif

/*
 * Function:  Process_InitParallelHandling
 * --------------------
 *  Inicializa todo lo necesario para manejar correctamente los procesos hijo.
 *
 *  returns: -1 si hay error, 0 si se inicializo sin error
 */
int Process_InitParallelHandling(int threadsMax){
    #if IS_OVER_POSIX2001
    signal(SIGCHLD, SIG_IGN);
    #else
    struct sigaction sa;
    sa.sa_handler = (__sighandler_t)SigChildHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, 0) == -1) {
        perror("Adding SIGCHLD Handler");
        exit(-1);
    }
    #endif
    sa.sa_handler = (__sighandler_t)ExitHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGTERM, &sa, 0) == -1) {
        perror("Adding SIGTERM Handler");
        exit(-1);
    }
    sa.sa_handler = (__sighandler_t)ExitHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, 0) == -1) {
        perror("Adding SIGINT Handler");
        exit(-1);
    }

    ThreadsMax = threadsMax;
    if(pthread_mutex_init(&ActiveThreadsLock, NULL) != 0) {
        perror("Parallel Processing create lock");
        exit(-1);
    }
    pthread_mutex_lock(&ActiveThreadsLock);
    ActiveThreads = 0;
    pthread_mutex_unlock(&ActiveThreadsLock);

    return 0;
}

/*
 * Function:  Process_UpdateMaxThreads
 * --------------------
 *  Cambio el maximo posible de threads.
 *
 *  returns: void.
 */
void Process_UpdateMaxThreads(int threadsMax) {
    pthread_mutex_lock(&ActiveThreadsLock);
    ThreadsMax = threadsMax;
    pthread_mutex_unlock(&ActiveThreadsLock);
}

/*
 * Function:  Process_ExitThread
 * --------------------
 *  Exit a ejecutar por los threads para desocupar el lugar que ocupan.
 *
 *  returns: void;
 */
void Process_ExitThread(void* returnVal) {
    pthread_mutex_lock(&ActiveThreadsLock);
    ActiveThreads--;
    pthread_mutex_unlock(&ActiveThreadsLock);

    pthread_exit(returnVal);
}

/*
 * Function:  Process_NewThread
 * --------------------
 *  Crea un nuevo thread, si no se excedio el maximo configurado
 *
 *  returns: 0 si pudo crear el thread, -1 si no fue posible.
 */
int Process_NewThread(void *(*start_routine) (void *), void *arg) {
    pthread_t tid;
    pthread_mutex_lock(&ActiveThreadsLock);
    if(ActiveThreads >= ThreadsMax) {
        pthread_mutex_unlock(&ActiveThreadsLock);
        return -1;
    }
    ActiveThreads++;
    pthread_mutex_unlock(&ActiveThreadsLock);
    if(pthread_create(&tid, NULL, start_routine, arg) != 0) {
        perror("Create Thread");
        pthread_mutex_lock(&ActiveThreadsLock);
        ActiveThreads--;
        pthread_mutex_unlock(&ActiveThreadsLock);
        return -1;
    }
    pthread_detach(tid);

    return 0;
}

/*
 * Function:  Process_StartChild
 * --------------------
 *  Crea un nuevo proceso hijo, chequeando errores, y haciendo exit si falla
 * luego retorna la id del child o 0.
 *
 *  returns: en el padre 0, en el child el id
 */
int Process_StartChild(CHILD_TYPE type) {
    int childId = 0;
    ChildProcess* newChild;
    struct sigaction sa;

    childId = fork();

    switch (childId)
    {
    case -1:
        perror("Fork new Process");
        exit(-1);
        break;
    case 0:
        // si es un child que voy a tracker y que pretendo que nadie lo mate hago override de SIGTERM
        if(type == TRACKED) {
            sa.sa_handler = SIG_IGN;
            sigemptyset(&sa.sa_mask);
            if (sigaction(SIGTERM, &sa, 0) == -1) {
                perror("Adding child SIGTERM Handler");
                exit(-1);
            }
        }
        break;
    default:
        if(type == TRACKED) {
            newChild = malloc(sizeof(ChildProcess));
            newChild->pid = childId;
            newChild->previous = TrackedChildsEnd;
            TrackedChildsEnd = newChild;
        }
        break;
    }   
    return childId;
}

/*
 * Function:  Process_TearDown
 * --------------------
 *  Les envia la signal SIGINT a los procesos hijo que lleva trackeados y desaloca las estructuras de manejo de hijos.
 *
 *  returns: void
 */
void Process_TearDown(void) {
    ChildProcess *i = TrackedChildsEnd;
    ChildProcess *aux = TrackedChildsEnd;
    while(i != NULL) {
        kill(i->pid, SIGINT);
        aux = i;
        i = i->previous;
        free(aux);
    }
    TrackedChildsEnd = NULL;

    // bloqueo la creacion de mas threads
    pthread_mutex_lock(&ActiveThreadsLock);
    ActiveThreads = ThreadsMax;
    pthread_mutex_unlock(&ActiveThreadsLock);
    pthread_mutex_destroy(&ActiveThreadsLock);

    return;
}

void Process_SignalTrackedChilds(int signal) {
    ChildProcess *i = TrackedChildsEnd;
    while(i != NULL) {
        kill(i->pid, signal);
        i = i->previous;
    }
    return;
}
