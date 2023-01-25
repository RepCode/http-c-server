#ifndef FILE_HANDLER
#define FILE_HANDLER

typedef struct FileLineStruct {
    char* line;
    int size;
} FileLine;

int CopyResponseFile(char* path, char** buffer, int startIndex);
void SetBasePath(char* programPath);
int GetEnvironmentFile(FileLine** fileLines);
char* GetFileAssetsPath(void);

#endif