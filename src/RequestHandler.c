#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AppConfig.h"
#include "RequestHandler.h"
#include "FileHandler.h"

typedef struct MiddlewareStruct {
    char* path;
    void (*handler) (char**, int*);
    struct MiddlewareStruct* next;
} Middleware;

Middleware* MiddlewareListStart = NULL;

static int ApplyMiddleware(char* path, char** response, int* size);
static int SetContentType(char* path, char** responseBuffer);

/*
 * Function:  HandleRequest
 * --------------------
 *  Parsea la request, devuelve la response acorde en el buffer provisto y retorna su tamaño.
 *
 *  requestBuffer: puntero al comienzo del char array de la request recibida.
 *  responseBuffer: puntero a puntero de la respuesta a devolver.
 *
 *  returns: el tamaño de bytes escritos en el buffer de respuesta.
 */
int HandleRequest(char* requestBuffer, char** responseBuffer) {
    char *path;
    int responseSize, aux;
    char indexPath[] = "/index.html";
    char okResponseHeader[] = "HTTP/1.1 200 OK\r\n";
    char errorResponse[] = "HTTP/1.1 500 Server Error\r\n";

    switch(ParseRequest(requestBuffer, &path)) {
        case GET:
            *responseBuffer = malloc(sizeof(okResponseHeader));
            if(*responseBuffer == NULL) {
                return -1;
            }
            strcpy(*responseBuffer, okResponseHeader);
            responseSize = sizeof(okResponseHeader)-1;
            if(strlen(path) == 1) {
                path = malloc(sizeof(indexPath));
                if(path == NULL) {
                    return -1;
                }
                strcpy(path, indexPath);
            }
            aux = SetContentType(path, responseBuffer);
            if(aux == -1) {
                return -1;
            }
            responseSize += aux;

            aux = CopyResponseFile(path, responseBuffer, responseSize);
            if(aux == -1) {
                return -1;
            }
            responseSize += aux;
            if(ApplyMiddleware(path, responseBuffer, &responseSize) == -1) {
                return -1;
            }
            break;
        default:
            *responseBuffer = errorResponse;
            responseSize = sizeof(errorResponse);
            break;
    }
    free(path);

    return responseSize;
}

/*
 * Function:  ParseRequest
 * --------------------
 *  Parsea la request, devuelve la response acorde en el buffer provisto y retorna su tamaño.
 *
 *  requestBuffer: puntero al comienzo del char array de la request recibida.
 *  path: puntero a puntero del path de la request.
 *  pathSize: tamaño del array path.
 *
 *  returns: el tipo de request recibida.
 */
HTTP_REQUEST ParseRequest(char* requestBuffer, char** path) {
    int i = 0;
    int aux = 0;
    char method[20] = {0};
    int pathSize;
    while(requestBuffer[i] != ' ' && i < MAX_REQUEST_SIZE){
        method[i] = requestBuffer[i];
        i++;
    }
    if(strncmp(method, "GET", 2) == 0) {
        i++;
        aux = i;
        while(requestBuffer[i] != ' ' && i < MAX_REQUEST_SIZE){
            i++;
        }
        pathSize = i-aux;
        *path = malloc(sizeof(char) * (pathSize + 1)); // +1 por el \0
        if(*path == NULL) {
            return -1;
        }
        strncpy(*path, requestBuffer+aux, pathSize);
        (*path)[pathSize] = '\0';
        printf("GET Request: %s\n", *path);
        return GET;
    }

    return UNSUPPORTED;
}

/*
 * Function:  Server_AddResponseMiddleware
 * --------------------
 *  Agrega Middleware para que se ejecute cuando se hace una request al path indicado.
 *
 *  returns: void
 */
void Server_AddResponseMiddleware(char* path, void (*handler) (char**, int* )) {
    Middleware* index = MiddlewareListStart;
    if(MiddlewareListStart == NULL) {
        MiddlewareListStart = malloc(sizeof(Middleware));
        MiddlewareListStart->next = NULL;
        index = MiddlewareListStart;
    } else {
        while(index->next != NULL) {
            index = index->next;
        }
        index->next = malloc(sizeof(Middleware));
        index = index->next;
    }
    index->handler = handler;
    index->path = path;

    return;
}

/*
 * Function:  ApplyMiddleware
 * --------------------
 *  Busca en la lista de Middlewares uno que haga match con el path de la request actual y ejecuta su handler sobre la response de la request.
 *
 *  returns: void
 */
static int ApplyMiddleware(char* path, char** response, int* size) {
    Middleware* index = MiddlewareListStart;

    while(index != NULL) {
        if(strcmp(path, index->path) == 0) {
            index->handler(response, size);
        }
        index = index->next;
    }

    return 0;
}

#define SVG_CONTENT "Content-Type: image/svg+xml; charset=UTF-8\r\n\r\n"
#define HTML_CONTENT "Content-Type: text/html; charset=UTF-8\r\n\r\n"
#define ICON_CONTENT "Content-Type: image/x-icon; charset=UTF-8\r\n\r\n"
#define CSS_CONTENT "Content-Type: text/css; charset=UTF-8\r\n\r\n"

static int SetContentType(char* path, char** responseBuffer) {
    int pathLen = strlen(path);
    int i = 0;
    char type[10];

    while(path[i] != '.' && i < pathLen) {
        i++;
    }
    strcpy(type, path+i+1);
    if(strcmp(type, "svg") == 0) {
        *responseBuffer = realloc(*responseBuffer, strlen(*responseBuffer)*sizeof(char) + sizeof(SVG_CONTENT));
        if(*responseBuffer == NULL) {
            return -1;
        }
        strcat(*responseBuffer, SVG_CONTENT);
        return sizeof(SVG_CONTENT)/sizeof(char);
    }
    if(strcmp(type, "html") == 0) {
        *responseBuffer = realloc(*responseBuffer, strlen(*responseBuffer)*sizeof(char) + sizeof(HTML_CONTENT));
        if(*responseBuffer == NULL) {
            return -1;
        }
        strcat(*responseBuffer, HTML_CONTENT);
        return sizeof(HTML_CONTENT)/sizeof(char);
    }
    if(strcmp(type, "ico") == 0) {
        *responseBuffer = realloc(*responseBuffer, strlen(*responseBuffer)*sizeof(char) + sizeof(ICON_CONTENT));
        if(*responseBuffer == NULL) {
            return -1;
        }
        strcat(*responseBuffer, ICON_CONTENT);
        return sizeof(ICON_CONTENT)/sizeof(char);
    }
    if(strcmp(type, "css") == 0) {
        *responseBuffer = realloc(*responseBuffer, strlen(*responseBuffer)*sizeof(char) + sizeof(CSS_CONTENT));
        if(*responseBuffer == NULL) {
            return -1;
        }
        strcat(*responseBuffer, CSS_CONTENT);
        return sizeof(CSS_CONTENT)/sizeof(char);
    }
    return -1; // unsuported media type
}
