#ifndef FILEIO
#define FILEIO

#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/stat.h>
#include "stdIO.h"


#include<stdio.h>

#define FIEL_OPEN_ERR "cannot open the file\n"
#define FILE_WRITE_ERR "cannot write to the file\n"
#define FILE_READ_ERR "cannot read file\n"

struct File_Pair {
    void* file;
    long long size;
};

void writeFile(char* filePath,void* buf,size_t size){
    int fileDescriptor = open(filePath,O_WRONLY|O_CREAT|O_TRUNC,0777);
    if(fileDescriptor==-1){
        print_stdout(FIEL_OPEN_ERR);
        return;
    }
    size_t status = write(fileDescriptor,buf,size);
    if(status<0){
        print_stdout(FILE_WRITE_ERR);
        close(fileDescriptor);
        return;
    }
    close(fileDescriptor);
}

struct File_Pair readFile(char* filePath){
    int fileDescriptor = open(filePath,O_RDONLY);
    struct File_Pair pair;
    if(fileDescriptor==-1){
        print_stdout(FIEL_OPEN_ERR);
        pair.size = -1;
        return pair;
    }
    struct stat sb;
    stat(filePath,&sb);
    void* buf = malloc(sb.st_size);
    long long status = read(fileDescriptor,buf,sb.st_size);
    if(status!=sb.st_size){
        print_stdout(FILE_READ_ERR);
        close(fileDescriptor);
        pair.size = -1;
        return pair;
    }
    close(fileDescriptor);
    pair.file = buf;
    pair.size = sb.st_size;
    return pair;
}

int isFileExist(char* path){
    if(access(path,F_OK)==0){
        return 1;
    }
    else{
        return -1;
    }
}

#endif