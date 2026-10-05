#ifndef TEST_TEMP_H
#define TEST_TEMP_H
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define TEST_RMDIR _rmdir
#define TEST_MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define TEST_RMDIR rmdir
#define TEST_MKDIR(path) mkdir(path,0755)
#endif
static inline bool testTempDirectory(char *path,size_t size,const char *prefix)
{
#ifdef _WIN32
    char base[MAX_PATH]; if (!GetTempPathA(sizeof(base),base)) return false;
    snprintf(path,size,"%ssynth-%s-%lu-%llu",base,prefix,(unsigned long)GetCurrentProcessId(),(unsigned long long)GetTickCount64());
    return CreateDirectoryA(path,NULL)!=0;
#else
    snprintf(path,size,"/tmp/synth-%s-XXXXXX",prefix);
    return mkdtemp(path)!=NULL;
#endif
}
static inline bool testFileExists(const char *path) { struct stat st; return stat(path,&st)==0; }
#endif
