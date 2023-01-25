#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h> 
#include <signal.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

#include "AppConfig.h"
#include "ParallelProcessing.h"
#include "DriverReader.h"
#include "Environment.h"
#include "gnuplot_i.h"
#include "FileHandler.h"
#include "bmpTypes.h"

#define R_SEM_INDEX 0
#define W_SEM_INDEX 1

int DriverReadFrequency_ms = DEFAULT_SENSOR_READ_FREQ_MS;
int DriverReadersMax = DEFAULT_THREADS_MAX;
int DriverFilterCount = DEFAULT_SENSOR_FILTER_COUNT;

static int DriverSemId;

DriverValue* DriverCurrentFilteredValue;

static char* HistoryFileNamePath;
char* HistoryFileFolder;
static unsigned long long int CurrentMS = 0;

struct BmpCalibrationValues CalibrationValues;

int CreateSharedMemorySpace();
void CreateDriverSemaphores();
void CreateHistoryFile();
int UpdateHistoryFile(DriverValue value);

void __attribute__((weak))DriverReader_BeforeRead() {
    return;
}

/*
 * Function:  DriverReader_Start
 * --------------------
 *  Crea un nuevo proceso que lee el driver constantemente
 *
 *  returns: void
 */
void DriverReader_Start(void) {
    #if (SIMULATE_DRIVER == 0)
    int driverFd;
    #endif
    int shmid;
    struct sembuf wsops = { .sem_op = -1, .sem_num = W_SEM_INDEX };
    struct sembuf rsops = { .sem_op = -DriverReadersMax, .sem_num = R_SEM_INDEX };
    int currentFilterCount;
    int32_t* filterValues;
    DriverValue nextValue;
    int index = 0;
    int i = 0;
    struct timespec time, rem;
    unsigned long long int rawValue;

    shmid = CreateSharedMemorySpace();
    CreateDriverSemaphores();

    currentFilterCount = DriverFilterCount;
    filterValues = calloc(currentFilterCount, sizeof(int32_t));
    CreateHistoryFile();

    // Necesario para que funcione gnuplot en la beaglebone black
    putenv("DISPLAY=:0.0");

    #if (SIMULATE_DRIVER == 0)
    driverFd = open(DRIVER_PATH, O_RDWR);
    if(driverFd < 0) {
        perror("Driver Open");
        exit(-1);
    }
    ioctl(driverFd, GET_CALIBRATION, &CalibrationValues);
    printf("Calib1: %d\n", CalibrationValues.Cal1);
    printf("Calib2: %d\n", CalibrationValues.Cal2);
    printf("Calib3: %d\n", CalibrationValues.Cal3);
    // hago una lectura inicial para llenar el array y evitar una primera medicion basura.
    read(driverFd, &rawValue, sizeof(int32_t));
    for(i=0; i<currentFilterCount; i++) {
        filterValues[i] = rawValue;
    }
    #endif

    if(!Process_StartChild(TRACKED)) {
        while(IsProcessRunning()) {
            DriverReader_BeforeRead();

            #if (SIMULATE_DRIVER == 0)
            read(driverFd, &filterValues[index], sizeof(int32_t));
            #else
            filterValues[index] = rand() / (rand()+1);
            #endif

            printf("El driver devolvió: %d\n", filterValues[index]);
            index = (index+1) % currentFilterCount;
            rawValue = 0;
            for(i=0; i<currentFilterCount; i++) {
                rawValue += filterValues[i];
            }
            rawValue /= currentFilterCount;

            nextValue.temperature = DriverReader_CalculateTemperature(rawValue);

            // bloqueo la escritura antes de actualizar el valor en memoria compartida.
            wsops.sem_op = -1;
            rsops.sem_op = -DriverReadersMax;
            semop(DriverSemId, &wsops, 1);
            semop(DriverSemId, &rsops, 1);
            
            DriverCurrentFilteredValue->temperature = nextValue.temperature;

            wsops.sem_op = 1;
            rsops.sem_op = DriverReadersMax;
            semop(DriverSemId, &rsops, 1);
            semop(DriverSemId, &wsops, 1);

            UpdateHistoryFile(nextValue);
            CurrentMS += DriverReadFrequency_ms;

            time.tv_sec = DriverReadFrequency_ms / 1000;
            time.tv_nsec = (DriverReadFrequency_ms % 1000L) * 1000000L;
            while(nanosleep(&time, &rem) == 1) {
                time = rem;
            }
            if(currentFilterCount != DriverFilterCount) {
                currentFilterCount = DriverFilterCount;
                free(filterValues);
                filterValues = malloc(sizeof(int32_t) * currentFilterCount);
                for(i=0; i<currentFilterCount; i++) {
                    filterValues[i] = rawValue;
                }
            }
        }
        // desinicializo todo
        close(driverFd);
        shmctl(shmid, IPC_RMID, NULL);
        semctl(DriverSemId, R_SEM_INDEX, IPC_RMID);
        semctl(DriverSemId, W_SEM_INDEX, IPC_RMID);
        free(HistoryFileFolder);
        
        free(filterValues);

        exit(0);
    }

    return;
}

/*
 * Function:  DriverReader_CalculateTemperature
 * --------------------
 *  Funcion transformar el valor crudo del sensor en una temperatura, con la formula que provee el sensor.
 *
 *  returns: void
 */
float DriverReader_CalculateTemperature(int32_t rawValue) {
    int32_t val1, val2, fine;

    val1 = ((((rawValue >> 3) - ((int32_t)CalibrationValues.Cal1 << 1))) *
    ((int32_t)CalibrationValues.Cal2)) >>
    11;

    val2 = (((((rawValue >> 4) - ((int32_t)CalibrationValues.Cal1)) *
                ((rawValue >> 4) - ((int32_t)CalibrationValues.Cal1))) >>
            12) *
            ((int32_t)CalibrationValues.Cal3)) >>
            14;

    fine = val1 + val2;

    return ((fine * 5 + 128) >> 8) / 100.0;
}


/*
 * Function:  DriverReader_GetCurrent
 * --------------------
 *  Funcion para leer de forma segura el valor actual del sensor.
 *
 *  returns: Estructura con el valor del sensor
 */
DriverValue DriverReader_GetCurrent() {
    DriverValue value;
    struct sembuf wsops = { .sem_op = -1, .sem_num = W_SEM_INDEX };
    struct sembuf rsops = { .sem_op = -1, .sem_num = R_SEM_INDEX };
    semop(DriverSemId, &wsops, 1); // bloqueante, si se esta escribiendo el nuevo valor se queda esperando
    semop(DriverSemId, &rsops, 1);
    wsops.sem_op = 1;
    semop(DriverSemId, &wsops, 1);

    value = *DriverCurrentFilteredValue;

    rsops.sem_op = 1;
    semop(DriverSemId, &rsops, 1);

    return value;
}

/*
 * Function:  CreateSharedMemorySpace
 * --------------------
 *  Crea seccion de memoria compartida donde se guarda la informacion del sensor.
 *
 *  returns: shmid del area de memoria compartida creada
 */
int CreateSharedMemorySpace() {
    key_t key;
    int shmid;

    key = ftok(KEY_FILE_PATH, DRIVER_SHM_CHAR);
    shmid = shmget(key, sizeof(DriverValue), 0644 | IPC_CREAT);
    DriverCurrentFilteredValue = shmat(shmid, NULL, 0);
    if(DriverCurrentFilteredValue == (DriverValue *)(-1)) {
        perror("shmat");
        exit(-1);
    }

    return shmid;
}

/*
 * Function:  CreateDriverSemaphores
 * --------------------
 *  Crea los semaforos para controlar la lectura y escritura de la memoria compartida
 *
 *  returns: void
 */
void CreateDriverSemaphores() {
    key_t key;
    int val = DriverReadersMax;

    key = ftok(KEY_FILE_PATH, DRIVER_SEM_CHAR);
    DriverSemId = semget(key, 2, 0644 | IPC_CREAT);
    if(DriverSemId < 0) {
        perror("Semaphores Create");
        exit(-1);
    }
    if(semctl(DriverSemId, R_SEM_INDEX, SETVAL, val) == -1) {
        perror("Read Semaphore Init");
        exit(-1);
    }
    val = 1;
    if(semctl(DriverSemId, W_SEM_INDEX, SETVAL, val) == -1) {
        perror("Write Semaphore Init");
        exit(-1);
    }
}

void CreateHistoryFile() {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    struct stat sb;
    FILE* fd;
    
    if(stat(HistoryFileFolder, &sb) == -1) {
        mkdir(HistoryFileFolder, 0700);
    }
    HistoryFileNamePath = malloc(strlen(HistoryFileFolder)*sizeof(char) + 25);
    sprintf(HistoryFileNamePath, "%s/%d-%02d-%02d:%02d:%02d:%02d.txt", HistoryFileFolder, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    printf("%s\n",HistoryFileNamePath);

    fd = fopen(HistoryFileNamePath, "w");
    if(fd == NULL) {
        perror("Create history file");
        exit(-1);
    }

    fclose(fd);
}

int UpdateHistoryFile(DriverValue value) {
    static unsigned long long measuresCount=0;
    gnuplot_ctrl * h ;
    char* assets = NULL;
    struct flock fl;
    int graph_fd;

    fl.l_type   = F_WRLCK;  /* F_RDLCK, F_WRLCK, F_UNLCK    */
    fl.l_whence = SEEK_SET; /* SEEK_SET, SEEK_CUR, SEEK_END */
    fl.l_start  = 0;        /* Offset from l_whence         */
    fl.l_len    = 0;        /* length, 0 = to EOF           */
    fl.l_pid    = getpid(); /* our PID                      */

    FILE* fd = fopen(HistoryFileNamePath, "a");
    if(fd == NULL) {
        perror("Driver values file open");
        exit(-1);
    }
    fprintf(fd, "%lld %f\n", CurrentMS, value.temperature);
    measuresCount++;
    fclose(fd);

    if(CurrentMS > DriverReadFrequency_ms) {
        assets = GetFileAssetsPath();
        assets = realloc(assets, (strlen(assets) + 1) * sizeof(char) + sizeof(HISTORIC_FILE_NAME));
        strcat(assets, "/");
        strcat(assets, HISTORIC_FILE_NAME);

        // chequeo que exista (si no existe no hay nada que lockear)
        if( access(assets, F_OK) != -1 ) {
            graph_fd = open(assets, O_WRONLY);
            if(graph_fd == -1) {
                printf("Open graph file");
                return -1;
            }
            if(fcntl(graph_fd, F_SETLKW, &fl) == -1) {
                printf("Write file lock");
                return -1;
            }
        }

        h = gnuplot_init() ;
        if(h != NULL) {
            gnuplot_cmd(h, "set terminal svg size 960, 480");
            gnuplot_cmd(h, "set output \"%s\"", assets);
            gnuplot_cmd(h, "set border lw 3 lc rgb \"white\"");
            gnuplot_cmd(h, "set xtics textcolor rgb \"white\"");
            gnuplot_cmd(h, "set ytics textcolor rgb \"white\"");
            gnuplot_cmd(h, "set xlabel \"Tiempo antes la última medición [sec]\" textcolor rgb \"white\"");
            gnuplot_cmd(h, "set ylabel \"Temperatura\" textcolor rgb \"white\"");
            gnuplot_cmd(h, "set key textcolor rgb \"white\"");

            gnuplot_cmd(h, "plot '%s' using (($1-%lld)/1000):($2) every %d::1 w l notitle", HistoryFileNamePath, CurrentMS, 10*(measuresCount/1000)+1);

            gnuplot_close(h);
        }

        fl.l_type   = F_UNLCK;  /* tell it to unlock the region */
        if(fcntl(graph_fd, F_SETLK, &fl) == -1) { /* set the region to unlocked */
            printf("Write file unlock");
            return -1;
        }
        close(graph_fd);
        free(assets);
    }
    return 0;
}
