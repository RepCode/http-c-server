#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "AppConfig.h"
#include "Server.h"
#include "FileHandler.h"
#include "DriverReader.h"
#include "ParallelProcessing.h"
#include "Middlewares.h"
#include "Environment.h"

int Port = DEFAULT_PORT;

extern int DriverReadFrequency_ms;
extern int DriverReadersMax;
extern int DriverFilterCount;
extern char* HistoryFileFolder;

/*
 * Function:  SensorMiddleware
 * --------------------
 *  Request Middleware que busca en la response la keyword donde va el valor del sensor y lo cambia por la lectura del driver
 *
 *  returns: void
 */
void SensorMiddleware(char** response, int *responseSize) {
    char sensorValue[20] = { 0 };
    sprintf(sensorValue, "%.2f", DriverReader_GetCurrent().temperature);
    ReplaceKeywordInResponseMiddleware(HTML_SENSOR_KEYWORD, sensorValue, response, responseSize);
}

void DriverReader_BeforeRead() {
    if(!EnvironmentVariables_IsUpToDate()) {
        EnvironmentVariables_Set();
        EnvironmentVariables_GetInt(SENSOR_READ_FREQ_MS, &DriverReadFrequency_ms);
        EnvironmentVariables_GetInt(THREADS_MAX, &DriverReadersMax);
        EnvironmentVariables_GetInt(SENSOR_FILTER_COUNT, &DriverFilterCount);
    }

    return;
}

/*
 * Function:  Server_Running
 * --------------------
 *  Override de la funcion default del Server para saber si seguir corriendo.
 *
 *  returns: bool indicando si debe seguir corriendo o no.
 */
bool Server_Running() {
    return IsProcessRunning() && EnvironmentVariables_IsUpToDate();
}

int main(int argc, char *argv[])
{
    int serverBacklog = DEFAULT_TCP_BACKLOG;
    int threadsMax = DEFAULT_THREADS_MAX;
    // seteo el base path desde el cual referenciar archivos.
    SetBasePath(argv[0]);

    // inicializo las variables de ambiente, leidas del archivo environment.config
    EnvironmentVariables_Init();

    EnvironmentVariables_GetInt(THREADS_MAX, &threadsMax);
    Process_InitParallelHandling(threadsMax);

    // cambio el puerto default si se pasa por command line
    if(1 < argc){
        Port = atoi(argv[1]);
    }

    // Driver Setup
    EnvironmentVariables_GetInt(SENSOR_READ_FREQ_MS, &DriverReadFrequency_ms);
    EnvironmentVariables_GetInt(THREADS_MAX, &DriverReadersMax);
    EnvironmentVariables_GetInt(SENSOR_FILTER_COUNT, &DriverFilterCount);
    EnvironmentVariables_GetDinamicString(HISTORY_PATH, &HistoryFileFolder);
    if(HistoryFileFolder == NULL) {
        HistoryFileFolder = malloc(sizeof(DEFAULT_HISTORY_PATH));
        strcpy(HistoryFileFolder, DEFAULT_HISTORY_PATH);
    }
    // inicializo el process para leer el driver
    DriverReader_Start();

    // Agrego middleware que va a agregar la lectura del sensor en el HTML
    Server_AddResponseMiddleware("/index.html", SensorMiddleware);

    EnvironmentVariables_GetInt(TCP_BACKLOG, &serverBacklog);
    // inicializo el server
    while(IsProcessRunning()) {
        if(!EnvironmentVariables_IsUpToDate()) {
            EnvironmentVariables_Set();
            EnvironmentVariables_GetInt(TCP_BACKLOG, &serverBacklog);
            EnvironmentVariables_GetInt(THREADS_MAX, &threadsMax);
            Process_UpdateMaxThreads(threadsMax);
        }

        if(Server_Serve(Port, serverBacklog) < 0) {
            break;
        }
    }

    Process_TearDown();

    EnvironmentVariables_TearDown();

    exit(0);
}