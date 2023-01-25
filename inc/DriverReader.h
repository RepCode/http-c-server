#ifndef DRIVER_READER
#define DRIVER_READER

typedef struct DriverValueStruct{
    float temperature;
} DriverValue;

extern int DriverReadFrequency_ms;
extern int DriverReadersMax;
extern int DriverFilterCount;
extern char* HistoryFileFolder;

void DriverReader_Start(void);
DriverValue DriverReader_GetCurrent();
void DriverReader_BeforeRead();

float DriverReader_CalculateTemperature(int32_t);

#endif