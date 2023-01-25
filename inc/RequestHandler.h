#ifndef REQUEST_HANDLER
#define REQUEST_HANDLER

typedef enum Requests {
    UNSUPPORTED,
    GET,
} HTTP_REQUEST;


int HandleRequest(char* requestBuffer, char** responseBuffer);
HTTP_REQUEST ParseRequest(char* requestBuffer, char** path);

#endif