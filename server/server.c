
#include <sys/socket.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <errno.h>
#include <arpa/inet.h>

#include "../file/stdIO.h"
#include "../file/fileIO.h"
#include "../utils/utils.h"

#define QUEUE_SIZE 10
#define NO_FILE "NOFILE "
#define HEARTBEAT "4321"
#define UPLOAD_KEYWORD "UP"
#define DOWNLOAD_KEYWORD "DL"
#define EXIT_KEYWORD "end"
#define LOCALHOST "127.0.0.1"

#define SOCKET_SET_OPS_ERR "socket set options failed.\nexit\n"
#define SOCKET_CREAT_ERR "server init failed.\nexit\n"
#define SOCKET_BIND_ERR "socket bind failed.\nexit\n"
#define SOCKET_LISTEN_ERR "socket listen failed.\nexit\n"
#define ARGS_NUM_FAILED "invalid arguments.\nexit\n"
#define CONNECTION_LOST "client connection disconnected.\n"
#define HEARTBEAT_ENABLED "start sending heartbeat...\n"
#define LISTENING_ON_PORT "server listening on port "
#define NEW_CLIENT "new client connected.\n"
#define DOWNLOAD_REQ_SUCC "downlaod request recieved. file send...done\n"
#define DOWNLOAD_REQ_FAIL "download request recieved. file not found.\n"
#define UPLOAD_REQ_DONE "upload request received. file uploaded to server.\n"

struct network_pair UDPpair;

struct network_pair setupTCPSocket(int listenPort){
    int socketfd;
    int optval = 1,optval2 = 1;
    if((socketfd = socket(AF_INET,SOCK_STREAM,0)) == 0){
        print_stdout(SOCKET_CREAT_ERR);
        _exit(EXIT_FAILURE);
    }
    struct sockaddr_in* address = malloc(sizeof(struct sockaddr_in));
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = inet_addr(LOCALHOST);
    address->sin_port = htons(listenPort);
    if(setsockopt(socketfd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval))<0){
        print_stdout(SOCKET_SET_OPS_ERR);
        _exit(EXIT_FAILURE);
    }
    if(setsockopt(socketfd,SOL_SOCKET,SO_REUSEPORT,&optval2,sizeof(optval2))<0){
        print_stdout(SOCKET_SET_OPS_ERR);
        _exit(EXIT_FAILURE);
    }
    if(bind(socketfd,(struct sockaddr*)address,sizeof(*address))<0){
        print_stdout(SOCKET_BIND_ERR);
        _exit(EXIT_FAILURE);
    }
    if(listen(socketfd,QUEUE_SIZE)<0){
        print_stdout(SOCKET_LISTEN_ERR);
        _exit(EXIT_FAILURE);
    }
    struct network_pair pair;
    pair.socketfd = socketfd;
    pair.address = address;
    return pair;
}

struct network_pair setupUDPSocket(int broadcastPort){
    int socketfd;
    int broadcast = 1,opt = 1;
    if((socketfd = socket(AF_INET,SOCK_DGRAM,0)) == 0){
        print_stdout(SOCKET_CREAT_ERR);
        _exit(EXIT_FAILURE);
    }

    if(setsockopt(socketfd,SOL_SOCKET,SO_BROADCAST,&broadcast,sizeof broadcast)<0){
        print_stdout(SOCKET_SET_OPS_ERR);
        _exit(EXIT_FAILURE);
    }
    if(setsockopt(socketfd,SOL_SOCKET,SO_REUSEPORT,&opt,sizeof opt)<0){
        print_stdout(SOCKET_SET_OPS_ERR);
        _exit(EXIT_FAILURE);
    }
    struct sockaddr_in* address = malloc(sizeof(struct sockaddr_in));
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_BROADCAST;
    address->sin_port = htons(broadcastPort);
    memset(address->sin_zero, '\0', sizeof address->sin_zero);
    if(bind(socketfd,(struct sockaddr*)address,sizeof(*address))<0){
        print_stdout(SOCKET_BIND_ERR);
        _exit(EXIT_FAILURE);
    }
    struct network_pair pair;
    pair.socketfd = socketfd;
    pair.address = address;
    return pair;
}


void heartBeat(int signum){
    int s=sendto(UDPpair.socketfd,HEARTBEAT,strlen(HEARTBEAT),0,(struct sockaddr*)UDPpair.address,sizeof(*UDPpair.address));
    alarm(1);
    UDPpair.address->sin_addr.s_addr = INADDR_BROADCAST;
}

void UDPInit(){
    signal(SIGALRM,heartBeat);
    alarm(1);
}

int handleClientReq(int sockfd){
    char* temp = malloc(1024);
    char* data = malloc(1);
    int rf,flag = 1;
    do{
        rf = recv(sockfd,temp,1024,0);
        if(rf==0 && flag){
            return -1;
        }
        flag = 0;
        data = realloc(data,strlen(temp)+strlen(data));
        data = strcat(data,temp);
    }while(rf==1024);
    char* token = strtok(data," ");
    if(strcmp(token,DOWNLOAD_KEYWORD)==0){
        char* filename = strtok(NULL,"\0");
        if(isFileExist(filename)==1){
            struct File_Pair file = readFile(filename);
            int len = strlen(filename)+1+file.size+1;
            char* data = (char*) malloc(len);
            data = strcat(data,filename);
            data = strcat(data," ");
            data = strcat(data,(char*)file.file);
            data = strcat(data,"\0");
            send(sockfd,data,len,0);
            print_stdout(DOWNLOAD_REQ_SUCC);
        }
        else{
            print_stdout(DOWNLOAD_REQ_FAIL);
            send(sockfd,NO_FILE,strlen(NO_FILE),0);
        }
    }
    else if(strcmp(token,UPLOAD_KEYWORD)==0){
        char* filename = strtok(NULL," ");
        char* data = strtok(NULL,"\0");
        writeFile(filename,data,strlen(data));
        print_stdout(UPLOAD_REQ_DONE);
    }
    else if(strcat(token,EXIT_KEYWORD)==0){
        close(sockfd);
        return -1;
    }
    return 1;
}

int main(int argc,char** argv){
    if(argc != 2){
        print_stdout(ARGS_NUM_FAILED);
        exit(EXIT_FAILURE);
    }
    UDPpair = setupUDPSocket(atoi(argv[1]));
    print_stdout(HEARTBEAT_ENABLED);
    UDPInit();
    struct network_pair pair = setupTCPSocket(atoi(HEARTBEAT));
    print_stdout(LISTENING_ON_PORT);print_stdout(HEARTBEAT);print_stdout("\n");
    int *socks = malloc(2*sizeof(int));
    int maxfd = pair.socketfd,socksNum=2,i;
    socks[0] = pair.socketfd;
    socks[1] = STDIN_FILENO;
    fd_set sockSet;
    while(1){
        struct timeval time;
        time.tv_sec = 0;
        time.tv_usec = 50000;
        FD_ZERO(&sockSet);
        maxfd = socks[0];
        for(i=0;i<socksNum;i++){
            if(socks[i]>=0){
                FD_SET(socks[i],&sockSet);
            }
            if(maxfd<socks[i]){
                maxfd = socks[i];
            }
        }
        int status = select(maxfd+1,&sockSet,NULL,NULL,&time);
        if(status!=0 && status!=-1){
            if(FD_ISSET(socks[0],&sockSet)){
                struct sockaddr_in addr;
                int size = sizeof(addr);
                int newfd = accept(pair.socketfd,(struct sockaddr*)&addr,&size);
                socksNum++;
                socks = addTovector(socks,newfd,socksNum-1);
                if(newfd>maxfd){
                    maxfd = newfd;
                }
                print_stdout(NEW_CLIENT);
                FD_SET(newfd,&sockSet);
                continue;
            }
            else{
                for(i=2;i<socksNum;i++){
                    if(FD_ISSET(socks[i],&sockSet)){
                        if(handleClientReq(socks[i])<0){
                            print_stdout(CONNECTION_LOST);
                            close(socks[i]);
                            socks = removeFromVector(socks,socks[i],socksNum);
                            socksNum--;

                        }
                        continue;
                    }
                }
            }
        }
    }
}


