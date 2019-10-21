#ifndef UTILS
#define UTILS

#include<math.h>
#include<stdlib.h>
#include<string.h>
#include<netinet/in.h>

struct network_pair{
    int socketfd;
    struct sockaddr_in* address;
};

int* addTovector(int* vector,int el,int size){
    int* temp = (int*) malloc(size+1);
    int i;
    for(i=0;i<size;i++){
        temp[i] = vector[i];
    }
    temp[size] = el;
    free(vector);
    return temp;
}

int* removeFromVector(int* vector,int el,int size){
    int* temp = (int*) malloc((size-1)*sizeof(int));
    int i;
    for(i=0;i<size && vector[i]!=el;i++){
        temp[i] = vector[i];
    }
    int j;
    for(j=i+1;j<size;j++){
        temp[j-1] = vector[j];
    }
    free(vector);
    return temp;
}

#endif