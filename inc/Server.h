#ifndef SERVER
#define SERVER

#include <stdbool.h>

bool Server_Running(void);

int Server_Serve(int port, int backlog);
void Server_AddResponseMiddleware(char* path, void (*handler) (char**, int* ));

#endif