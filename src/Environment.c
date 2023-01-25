#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>

#include "FileHandler.h"
#include "Environment.h"
#include "ParallelProcessing.h"

static EnvironmentVariable* EnvVariableArrayStart;
static int EnvVariableArrayLength = 0;

static bool EnvVariablesUpToDate = false;

/*
 * Function:  EnvironmentVariables_IsUpToDate
 * --------------------
 *  funcion para ver si estan actualizadas las variables de entorno;
 *
 *  returns: bool
 */
bool EnvironmentVariables_IsUpToDate(void) {
    return EnvVariablesUpToDate;
}

/*
 * Function:  EnvironmentVariables_UpdateHandler
 * --------------------
 *  Handler para indicar que hubo un update en el archivo de configuracion, por lo que las variables estan desactualizadas.
 *
 *  returns: void
 */
void EnvironmentVariables_UpdateHandler(void) {
    EnvVariablesUpToDate = false;
}

/*
 * Function:  EnvironmentVariables_Init
 * --------------------
 *  Inicializa todo lo necesario para el manejo de variables de entorno.
 *
 *  returns: void
 */
void EnvironmentVariables_Init(void) {
    struct sigaction sa;
    sa.sa_handler = (__sighandler_t)EnvironmentVariables_UpdateHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, 0) == -1) {
        perror("Adding SIGUSR1 Handler");
        exit(-1);
    }
    EnvironmentVariables_Set();
}

/*
 * Function:  EnvironmentVariables_Set
 * --------------------
 *  Parsea el archivo de environment y lo carga en memoria acorde a la estructura EnvironmentVariable para ser accesible por el resto del codigo
 *
 *  returns: void
 */
void EnvironmentVariables_Set() {
    FileLine* fileLines;
    int linesCount = 0;
    int i, j;
    char* auxArray;

    linesCount = GetEnvironmentFile(&fileLines);
    
    for(i=0; i<EnvVariableArrayLength; i++) {
        free(EnvVariableArrayStart[i].key);
    }
    free(EnvVariableArrayStart);
    EnvVariableArrayStart = malloc(sizeof(EnvironmentVariable) * linesCount); // asumo que todas las lineas van a ser variables asi hago malloc una sola vez
    EnvVariableArrayLength = 0;

    for(i=0; i<linesCount; i++) {
        if(fileLines[i].size > 2 && fileLines[i].line[0] == '/' && fileLines[i].line[1] == '/') { // si es un comentario
            continue;
        }
        auxArray = malloc(sizeof(char) * fileLines[i].size);
        j=0;
        while(fileLines[i].line[j] != '=') {
            if( j == fileLines[i].size) { // si llegue al final hay un error, porque no hay un = separando key de value.
                continue;
            }
            auxArray[j] = fileLines[i].line[j];
            j++;
        }
        auxArray[j] = '\0';
        (EnvVariableArrayStart+EnvVariableArrayLength)->key = auxArray;
        j++;
        (EnvVariableArrayStart+EnvVariableArrayLength)->value = auxArray+j;
        while(fileLines[i].line[j] != '\n' && fileLines[i].line[j] != '\r') {
            auxArray[j] = fileLines[i].line[j];
            j++;
        }
        auxArray[j] = '\0';
        EnvVariableArrayLength++;
    }
    if(EnvVariableArrayLength < linesCount) {
        // reduzco el array si no utilice todo.
        EnvVariableArrayStart = realloc(EnvVariableArrayStart, sizeof(EnvironmentVariable)*EnvVariableArrayLength);
    }
    free(fileLines);

    EnvVariablesUpToDate = true;

    Process_SignalTrackedChilds(SIGUSR1);

    return;
}

/*
 * Function:  EnvironmentVariables_GetDinamicString
 * --------------------
 *  Carga el puntero a char provisto con el valor de la key provista, encargandose de alocar la memoria necesaria.
 *
 *  returns: void
 */
int EnvironmentVariables_GetDinamicString(char* key, char** value) {
    int i;
    for(i=0; i < EnvVariableArrayLength ; i++) {
        if(strcmp(key, EnvVariableArrayStart[i].key) == 0) {
            *value = malloc(sizeof(char) * strlen(EnvVariableArrayStart[i].value));
            strcpy(*value, EnvVariableArrayStart[i].value);
            return 0;
        }
    }

    return -1;
}

/*
 * Function:  EnvironmentVariables_TearDown
 * --------------------
 *  Obtiene el valor de la key que se le pasa y lo transforma a entero. El entero provisto no es afectado si no se encuentra nada.
 *
 *  returns: 0 si se encontro la key, -1 ante un error
 */
int EnvironmentVariables_GetInt(char* key, int* value) {
    int i;
    for(i=0; i < EnvVariableArrayLength ; i++) {
        if(strcmp(key, EnvVariableArrayStart[i].key) == 0) {
            *value = atoi(EnvVariableArrayStart[i].value);
            return 0;
        }
    }

    return -1;
}

/*
 * Function:  EnvironmentVariables_TearDown
 * --------------------
 *  Libera los recursos utilizados para controlar las variables de entorno
 *
 *  returns: void
 */
void EnvironmentVariables_TearDown() {
    int i = 0;
    for(i=0; i<EnvVariableArrayLength; i++) {
        free(EnvVariableArrayStart[i].key);
    }
    free(EnvVariableArrayStart);
}
