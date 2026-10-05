#include "file_browser.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define GET_CWD _getcwd
#else
#include <dirent.h>
#include <unistd.h>
#define GET_CWD getcwd
#endif
static bool midiExtension(const char *name)
{
    const char *extension=strrchr(name,'.');
    if (!extension) return false;
    char lower[8]; size_t n=strlen(extension);
    if (n>=sizeof(lower)) return false;
    for (size_t i=0;i<=n;++i) lower[i]=(char)tolower((unsigned char)extension[i]);
    return !strcmp(lower,".mid") || !strcmp(lower,".midi");
}
static int compare(const void *a,const void *b)
{
    const BrowserEntry *x=a,*y=b;
    if (x->directory!=y->directory) return x->directory ? -1 : 1;
    return strcmp(x->name,y->name);
}
static void entry(FileBrowser *browser,const char *name,bool directory)
{
    if (!strcmp(name,".") || !strcmp(name,"..") || strlen(name)>=256 || browser->count==BROWSER_MAX) return;
    if (!directory && !midiExtension(name)) return;
    BrowserEntry *e=&browser->entries[browser->count++]; strcpy(e->name,name); e->directory=directory;
}
int fileBrowserOpen(FileBrowser *b,const char *directory,char *error,size_t size)
{
    FileBrowser next={0}; next.selected=-1;
    if (!directory) {
        if (!GET_CWD(next.directory,sizeof(next.directory))) { snprintf(error,size,"Cannot find current folder."); return -1; }
    } else {
        if (strlen(directory)>=sizeof(next.directory)) { snprintf(error,size,"Folder path is too long."); return -1; }
        strcpy(next.directory,directory);
    }
#ifdef _WIN32
    char pattern[BROWSER_PATH_MAX];
    if (snprintf(pattern,sizeof(pattern),"%s/*",next.directory)>=(int)sizeof(pattern)) return -1;
    WIN32_FIND_DATAA data; HANDLE handle=FindFirstFileA(pattern,&data);
    if (handle==INVALID_HANDLE_VALUE) { snprintf(error,size,"Cannot open folder."); return -1; }
    do { entry(&next,data.cFileName,(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0); } while (FindNextFileA(handle,&data));
    FindClose(handle);
#else
    DIR *dir=opendir(next.directory);
    if (!dir) { snprintf(error,size,"Cannot open folder."); return -1; }
    struct dirent *item;
    while ((item=readdir(dir))) {
        char path[BROWSER_PATH_MAX]; struct stat st;
        if (snprintf(path,sizeof(path),"%s/%s",next.directory,item->d_name)>=(int)sizeof(path)) continue;
        if (stat(path,&st)==0) entry(&next,item->d_name,S_ISDIR(st.st_mode));
    }
    closedir(dir);
#endif
    qsort(next.entries,(size_t)next.count,sizeof(*next.entries),compare);
    *b=next; return 0;
}
int fileBrowserChoose(FileBrowser *b,int selected,char *path,size_t pathSize,char *error,size_t size)
{
    if (selected==-1) {
        char parent[BROWSER_PATH_MAX]; strcpy(parent,b->directory);
        size_t n=strlen(parent);
        size_t root = n>=3 && parent[1]==':' ? 3 : 1;
        while (n>root && (parent[n-1]=='/' || parent[n-1]=='\\')) parent[--n]=0;
        while (n>root && parent[n-1]!='/' && parent[n-1]!='\\') parent[--n]=0;
        if (n>root) parent[n-1]=0;
        return fileBrowserOpen(b,parent,error,size);
    }
    if (selected<0 || selected>=b->count) { snprintf(error,size,"Select a MIDI file."); return -1; }
    if (snprintf(path,pathSize,"%s/%s",b->directory,b->entries[selected].name)>=(int)pathSize) {
        snprintf(error,size,"File path is too long."); return -1;
    }
    if (b->entries[selected].directory) return fileBrowserOpen(b,path,error,size);
    return 1;
}
