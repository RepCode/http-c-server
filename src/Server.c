#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>

#include "AppConfig.h"
#include "Server.h"
#include "RequestHandler.h"
#include "ParallelProcessing.h"
#include "Environment.h"

bool __attribute__((weak))Server_Running(void) {
    return true;
}

typedef struct RequestThreadArgsStruct {
    int fd_client;
} RequestThreadArgs;

void* RequestThread(void* args) {
    char requestBuffer[MAX_REQUEST_SIZE];
    char* responseBuffer = NULL;
    int responseSize = 0;
    int fd_client = ((RequestThreadArgs*)args)->fd_client;

    recv(fd_client, requestBuffer, MAX_REQUEST_SIZE-1, MSG_CMSG_CLOEXEC);
    responseSize = HandleRequest(requestBuffer, &responseBuffer);
    if(responseSize != -1) {
        write(fd_client, responseBuffer, responseSize-1);
    }

    free(responseBuffer);
    close(fd_client);
    free(args);

    Process_ExitThread(NULL);
    return NULL;
}

/*
 * Function:  Server_Serve
 * --------------------
 *  Servidor Concurrente que crea threads por cada request.
 *
 *  port: puerto en el que escuchar.
 *  backlog: cantidad de request que pueden quedar encoladas hasta que se les hace accept.
 *
 *  returns: 0 si no hay errores.
 */
int Server_Serve(int port, int backlog)
{
    struct sockaddr_in server_addr, client_addr;
    socklen_t sin_len = sizeof(client_addr);
    int fd_server, fd_client;
    int on = 1;
    struct pollfd fds[1];
    RequestThreadArgs* threadArgs;

    fd_server = socket(AF_INET, SOCK_STREAM, 0);
    if(fd_server < 0) {
        perror("Create socket endpoint");
        return -1;
    }

    setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);

    if(bind(fd_server, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("Bind socket to address and port");
        close(fd_server);
        return -1;
    }

    if(listen(fd_server, backlog) == -1) {
        perror("Listen");
        close(fd_server);
        return -1;
    }

    printf("Server listening on port: %d\n", port);

    fds[0].fd = fd_server;
    fds[0].events = POLLIN;
    while(1) {

        // Para no tener el proceso bloqueado hasta que llegue una request, lo polleo
        if(poll(fds, 1, POLL_TIMEOUT_MS) < 1) {
            if(Server_Running()) {
                continue;
            } else {
                // exit
                close(fd_server);
                break;
            }
        }

        fd_client = accept(fd_server, (struct sockaddr *) &client_addr, &sin_len);
        if(fd_client == -1) {
            perror("Connection failed");
        }
        threadArgs = malloc(sizeof(RequestThreadArgs));
        threadArgs->fd_client = fd_client;
        if(Process_NewThread(RequestThread, threadArgs) == -1) {
            close(fd_client);
        }
    }

    return 0;
}
