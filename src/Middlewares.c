#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ParallelProcessing.h"

/*
 * Function:  ReplaceKeywordInResponseMiddleware
 * --------------------
 *  Funcion generica para reemplazar keywords en un buffer, tiene en cuenta diferencias de tamaño entre el keyword
 * y su reemplazo, readaptando el tamaño del buffer y cambiando el valor reponseSize recibido por referencia.
 *
 *  returns: void
 */
void ReplaceKeywordInResponseMiddleware(char* keyWord, char* replacement, char** response, int* responseSize) {
    char* keywordStart;
    int index;
    int keyWordLen, replacementLen;
    while((keywordStart = strstr(*response, keyWord))) {
        index = keywordStart - *response; // trabajo con index porque keywordStart puede quedar invalidado
        replacementLen = strlen(replacement);
        keyWordLen = strlen(keyWord);
        if(replacementLen > keyWordLen) { // si hay que agrandar el string de respuesta
            *responseSize = *responseSize + replacementLen - keyWordLen;
            *response = realloc(*response, *responseSize * sizeof(char));
            if(*response == NULL) {
                Process_ExitThread(NULL);
            }
        }
        // copio la parte de la response que va despues del replacement al nuevo lugar.
        memmove(*response + index + replacementLen, *response + index + keyWordLen, (*responseSize - index - keyWordLen) * sizeof(char));
        if(replacementLen < keyWordLen) {
            *responseSize = *responseSize - keyWordLen + replacementLen;
        }
        strncpy(*response + index, replacement, replacementLen);
    }

    return;
}
