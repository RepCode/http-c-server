#ifndef ENVIRONMENT
#define ENVIRONMENT

#include <stdbool.h>

// se garantiza que ambos terminan en \0
typedef struct EnvironmentVariableStruct {
    char* key;
    char* value;
} EnvironmentVariable;

void EnvironmentVariables_Set(void);
void EnvironmentVariables_Init(void);
void EnvironmentVariables_TearDown(void);
int EnvironmentVariables_GetDinamicString(char* key, char** value);
int EnvironmentVariables_GetInt(char* key, int* value);
bool EnvironmentVariables_IsUpToDate(void);

#endif