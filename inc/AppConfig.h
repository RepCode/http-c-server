#ifndef APP_CONFIG
#define APP_CONFIG

#define DEFAULT_PORT    3000
#define MAX_REQUEST_SIZE 2000
#define ASSETS_REL_PATH "/assets"
#define ENVIRONMENT_REL_PATH "/environment.config"
#define HISTORIC_FILE_NAME "historic.svg"

// Env Variables
#define SENSOR_READ_FREQ_MS "SENSOR_READ_FREQ_MS"
#define TCP_BACKLOG "TCP_BACKLOG"
#define THREADS_MAX "THREADS_MAX"
#define SENSOR_FILTER_COUNT "SENSOR_FILTER_COUNT"
#define HISTORY_PATH "HISTORY_PATH"

// Defaults
#define DEFAULT_SENSOR_READ_FREQ_MS 1000
#define DEFAULT_TCP_BACKLOG 2
#define DEFAULT_THREADS_MAX 1000
#define DEFAULT_SENSOR_FILTER_COUNT 5
#define DEFAULT_HISTORY_PATH "history"

#define POLL_TIMEOUT_MS 500

// Tal cual dice el manual de linux:
// https://man7.org/linux/man-pages/man2/sigaction.2.html
// a partir de POSIX 2001 con ignorar explicitamente SIGCHLD alcanza para no generar zombies
// de todos modos lo implemento de ambas formas y uso la forma explicita en base a lo que se configure en este define.
#define IS_OVER_POSIX2001 0

#ifdef ARM_PROGRAM
#define DRIVER_PATH "/dev/i2c-td3-device"
#define KEY_FILE_PATH "/home/ubuntu/server/keys.txt"

#else
// x86
#define DRIVER_PATH "/home/rep/Documents/TD3/RepoMine/r5054-td3-Pereira-Nunez-Machado-Martin/2doCuatrimestre/driver"
#define KEY_FILE_PATH "/home/rep/Documents/TD3/RepoMine/r5054-td3-Pereira-Nunez-Machado-Martin/2doCuatrimestre/keys.txt"

#endif

#define DRIVER_SHM_CHAR 'D'
#define DRIVER_SEM_CHAR 'S'

#define HTML_SENSOR_KEYWORD "%%TEMPERATURE%%"

#define SIMULATE_DRIVER 0

#endif