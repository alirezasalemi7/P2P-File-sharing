#ifndef STDIO
#define STDIO
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#define INPUT_READ 128

void print_stdout(char* text){
    write(STDOUT_FILENO,text,strlen(text));
}

char* get_input(){
    char* s = (char*) malloc(INPUT_READ+1);
    while(read(STDIN_FILENO,s,INPUT_READ)==0);
    s[INPUT_READ] = '\0';
    return s;
}

#endif