#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "AppConfig.h"
#include "FileHandler.h"

static char* BasePath;
static char* AssetsPath;

/*
 * Function:  CopyResponseFile
 * --------------------
 *  Lee un archivo de la carpeta assets, especificado por el path y lo guarda en
 *  el buffer a partir del index de inicio.
 *
 *  path: puntero al comienzo del char array del path al archivo
 *  pathSize: largo del path array
 *  buffer: puntero a puntero char del buffer a llenar
 *  startIndex: index a partir del cual escribir
 *
 *  returns: el tamaño de bytes del archivo leído, -1 si hubo un error.
 */
int CopyResponseFile(char* path, char** buffer, int startIndex) {
    char *filePath = NULL;
    FILE *fp;
    int fileLength = 0;
    struct flock fl;

    filePath = malloc(sizeof(char)*(strlen(path) + strlen(AssetsPath) + 1));
    if(filePath == NULL) {
        perror("Path malloc");
        return -1;
    }
    strcpy(filePath, AssetsPath);
    strcat(filePath, path);

    fl.l_type   = F_RDLCK;  /* F_RDLCK, F_WRLCK, F_UNLCK    */
    fl.l_whence = SEEK_SET; /* SEEK_SET, SEEK_CUR, SEEK_END */
    fl.l_start  = 0;        /* Offset from l_whence         */
    fl.l_len    = 0;        /* length, 0 = to EOF           */
    fl.l_pid    = getpid(); /* our PID                      */

    fp = fopen(filePath, "r");
    if(fp == NULL) {
        printf("%s\n", filePath);
        perror("File open");
        return -1;
    }
    if(fcntl(fileno(fp), F_SETLKW, &fl) == -1) {
        perror("Read lock file");
        return -1;
    }

    fseek(fp, 0L, SEEK_END);
    fileLength = ftell(fp);
    rewind(fp);

    *buffer = realloc(*buffer, sizeof(char)*(startIndex + fileLength));
    if(*buffer == NULL) {
        perror("Buffer realloc");
        return -1;
    }

    fread((*buffer)+startIndex-1, sizeof(char), fileLength, fp);

    fl.l_type   = F_UNLCK;  /* tell it to unlock the region */
    if(fcntl(fileno(fp), F_SETLK, &fl)) { /* set the region to unlocked */
        perror("Read unlock file");
        return -1;
    }

    fclose(fp);

    return fileLength;
}

/*
 * Function:  SetBasePath
 * --------------------
 *  Request Configura el Path base de donde ir a buscar los assets, carpeta que tiene que estar al mismo nivel del programa y contiene todos los
 * archivos html, favicon, etx
 *
 *  returns: void
 */
void SetBasePath(char* programPath) {
    int index = 1;
    int length = strlen(programPath);
    char assetsPath[] = ASSETS_REL_PATH;
    while(programPath[length-index] != '/') {
        index++;
    }
    BasePath = malloc(length - index);
    if(BasePath == NULL) {
        perror("Base path malloc");
        exit(-1);
    }
    AssetsPath = malloc(length - index + sizeof(assetsPath));
    if(AssetsPath == NULL) {
        perror("Assets path malloc");
        exit(-1);
    }
    
    
    strncpy(BasePath, programPath, length - index);
    strcpy(AssetsPath, BasePath);
    strcat(AssetsPath, assetsPath);

    return;
}

/*
 * Function:  GetEnvironmentFile
 * --------------------
 *  Devuelve un array de estructuras con cada linea del archivo de configuracion. Alloca dinamicamente toda la memoria,
 *  por lo que quien la llama debe liberarla.
 *
 *  returns: lines amount
 */
#define GetEnvironmentFile_DEFAULT_ALLOC 15
int GetEnvironmentFile(FileLine** fileLines) {
    int i = 0;
    int lineSize = 0;
    FILE* fd;
    char* filePath = NULL;
    size_t size = 0;

    *fileLines = calloc(GetEnvironmentFile_DEFAULT_ALLOC, sizeof(FileLine)); // prealoco punteros para 15 lineas
    if(*fileLines == NULL) {
        perror("File array calloc");
        exit(-1);
    }
    filePath = malloc(strlen(BasePath) + sizeof(ENVIRONMENT_REL_PATH));
    if(*fileLines == NULL) {
        perror("File path malloc");
        exit(-1);
    }

    strcpy(filePath, BasePath);
    strcat(filePath, ENVIRONMENT_REL_PATH);

    fd = fopen(filePath, "r");
    if(fd == NULL) {
        perror("Environment file open");
        exit(-1);
    }

    (*fileLines)[0].line = NULL;
    lineSize = getline(&((*fileLines)[0].line), &size, fd);

    while(lineSize >= 0) {
        if(lineSize > 0) {
            (*fileLines)[i].size = lineSize;
            i++;
            // si excedo el default de lineas alocadas.
            if(i % GetEnvironmentFile_DEFAULT_ALLOC == 0) {
                *fileLines = realloc(*fileLines, (i / GetEnvironmentFile_DEFAULT_ALLOC + 1) * GetEnvironmentFile_DEFAULT_ALLOC * sizeof(FileLine));
                if(*fileLines == NULL) {
                    perror("File Lines array realloc");
                    exit(-1);
                }
            }
            (*fileLines)[i].line = NULL;
        }
        lineSize = getline(&((*fileLines)[i].line), &size, fd);
    }

    free(filePath);

    return i;
}

char* GetFileAssetsPath(void) {
    char* path = malloc(sizeof(char) * (strlen(AssetsPath) + 1));
    strcpy(path, AssetsPath);
    return path;
}
