#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/time.h>
#include<sys/select.h>
#include<sys/signal.h>

#include "../file/stdIO.h"
#include "../file/fileIO.h"
#include "../utils/utils.h"

#define SOCKET_SET_OPS_ERR "socket set options failed.\nexit\n"
#define SOCKET_CREAT_ERR "server init failed.\nexit\n"
#define SOCKET_BIND_ERR "socket bind failed.\nexit\n"
#define SOCKET_LISTEN_ERR "socket listen failed.\nexit\n"
#define ARGS_NUM_FAILED "invalid arguments passed.\nexit\n"
#define CONNECTION_LOST "user connection lost.\n"
#define SOCK_ERR "cannot make socket.\nexit\n"
#define SOCK_SET_OPS_ERR "cannot set options on socket.\nexit\n"
#define SOCK_BIND_ERR "cannot bind socket.\nexit\n"
#define ACTIVE_SEREVR "server is active. connected.\n"
#define NOT_ACTIVE_SERVER "server is not active. P2P enabled.\n"
#define SERVER_CONNECTION_FAILED "cannot connect to the server\n"
#define INVALID_INST "invalid instruction\n"
#define INVALID_FILE_PATH "file does not exist.\n"
#define SERVER_NO_FILE "this file is not available on server. broadcasting used to find file... \n"
#define FIEL_SAVED "file saved on disk.\n"
#define SERVER_OFF "server is down. p2p enabled.\n"
#define NEW_CLIENT_ADDED "a client connected to get file in p2p mode.\n"
#define FILE_FIND_P2P "broadcast was successful. connecting to owner...\n"
#define FILE_AVAILABLE_P2P "broadcast recieved. file found. answering...\n"
#define FILE_BROADCAST_NOT_FOUND "broadcast recieved. file not found.\n"
#define DOWNLOAD_REQUEST "client requested for file. file send.\n"
#define FILE_DOWNLOADED_P2P "broadcasted file recieved. saved on disk.\n"
#define P2P_DIRECT_CONNECTION_CLOSED "p2p direct connection closed.\n"
#define DATA_SEND_TO_SERVER "file send to sever...done.\n"
#define DOWNLOAD_REQ_SEND "download request send to the server.\n"
#define BROADCAST_SEND "broadcast message sent.\n"

#define EXIT_TOKEN "end"
#define UPLOAD_TOKEN "upload"
#define DOWNLOAD_TOKEN "download"
#define NO_FILE "NOFILE"
#define UPLOAD_KEYWORD "UP"
#define DOWNLOAD_KEYWORD "DL"
#define QUEUE_SIZE 5
#define BROAD_CAST_INTERVAL 3
#define ANSWER_KEYWORD "@ans"
#define LOCALHOST "127.0.0.1"

#define TIME_OUT_S 3
#define HEARTBET_LENGTH 10
#define CLIENT 0
#define SERVER 1

int activeServer = 0;
char* port_m;
int activeBroadcast = 0;
char* broadCastFileName;
char* serverReqFilename;
struct network_pair UDPPair;
struct network_pair TCPPair;

int isServerExist(int port){
    int sockfd;
    int opt = 1,opt2 = 1;
    if((sockfd = socket(AF_INET,SOCK_DGRAM,0))<0){
        print_stdout(SOCK_ERR);
        _exit(EXIT_FAILURE);
    }
    struct sockaddr_in *addr = malloc(sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_BROADCAST;
    addr->sin_port = htons(port);
    struct timeval timeOut;
    timeOut.tv_usec = TIME_OUT_S*100000;
    timeOut.tv_sec = TIME_OUT_S;
    if(setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&opt,sizeof(opt))<0){
        print_stdout(SOCK_SET_OPS_ERR);
        _exit(EXIT_FAILURE);
    }
    if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEPORT,&opt2,sizeof(opt2))<0){
        print_stdout(SOCK_SET_OPS_ERR);
        _exit(EXIT_FAILURE);
    }
    if(setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&timeOut,sizeof(timeOut))<0){
        print_stdout(SOCK_SET_OPS_ERR);
        _exit(EXIT_FAILURE);
    }
    if(bind(sockfd,(struct sockaddr*)addr,sizeof(*addr))<0){
        print_stdout(SOCK_BIND_ERR);
        _exit(EXIT_FAILURE);
    
    char* heartBeat = (char*) malloc(HEARTBET_LENGTH);
    heartBeat[0] = '\0';
    int size = sizeof(*addr);
    recvfrom(sockfd,heartBeat,HEARTBET_LENGTH,0,(struct sockaddr*)addr,&size);
    if(heartBeat[0]!='\0'){
        close(sockfd);
        return atoi(heartBeat);
    }
    else{
        close(sockfd);
        return -1;
    }
}
struct network_pair setTCPSocket(int port,int type){
    int sockfd;
    if((sockfd = socket(AF_INET,SOCK_STREAM,0))<0){
        print_stdout(SOCK_ERR);
        _exit(EXIT_FAILURE);
    }
    struct sockaddr_in *addr = malloc(sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = inet_addr(LOCALHOST);
    addr->sin_port = htons(port);
    if(connect(sockfd,(struct sockaddr*)addr,sizeof(*addr))<0){
        if(type==SERVER){
            print_stdout(SERVER_CONNECTION_FAILED);
            _exit(EXIT_FAILURE);
        }
        sockfd = -1;
    }
    struct network_pair pair;
    pair.address = addr;
    pair.socketfd = sockfd;
    return pair;
}

void sendToServer(struct network_pair pair,char* path){
    struct File_Pair file = readFile(path);
    if(file.size<0){
        return;
    }
    int len = strlen(UPLOAD_KEYWORD)+1+strlen(path)+1+file.size+1;
    char* data = malloc(len);
    data = strcat(data,UPLOAD_KEYWORD);
    data = strcat(data," ");
    data = strcat(data,path);
    data = strcat(data," ");
    data = strcat(data,(char*)file.file);
    data = strcat(data,"\0");
    send(pair.socketfd,data,len,0);
}

void downloadReq(struct network_pair pair,char* filename){
    int len = strlen(DOWNLOAD_KEYWORD)+1+strlen(filename)+1;
    char* data = (char*) malloc(len);
    data = strcat(data,DOWNLOAD_KEYWORD);
    data = strcat(data," ");
    data = strcat(data,filename);
    data = strcat(data,"\0");
    serverReqFilename = filename;
    send(pair.socketfd,data,len,0);
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
            int len = strlen(UPLOAD_KEYWORD)+1+strlen(filename)+1+file.size+1;
            char* data = (char*) malloc(len);
            data = strcat(data,UPLOAD_KEYWORD);
            data = strcat(data," ");
            data = strcat(data,filename);
            data = strcat(data," ");
            data = strcat(data,(char*)file.file);
            data = strcat(data,"\0");
            send(sockfd,data,len,0);
            print_stdout(DOWNLOAD_REQUEST);
        }
    }
    else if(strcmp(token,UPLOAD_KEYWORD)==0){
        char* filename = strtok(NULL," ");
        char* data = strtok(NULL,"\0");
        writeFile(filename,data,strlen(data));
        print_stdout(FILE_DOWNLOADED_P2P);
    }
    return 1;
}

void broadCastRequest(char* filename){
    char* port = port_m;
    int len = strlen(filename)+1+strlen(port)+1;
    char* data = (char*) malloc(len);
    data = strcat(data,filename);
    data = strcat(data," ");
    data = strcat(data,port);
    data = strcat(data,"\0");
    int stat = sendto(UDPPair.socketfd,data,len,0,(struct sockaddr*)UDPPair.address,sizeof(*UDPPair.address));
}

void broadCastHandler(int signum){
    if(activeBroadcast && broadCastFileName!=NULL){
        broadCastRequest(broadCastFileName);
        alarm(BROAD_CAST_INTERVAL);
        print_stdout(BROADCAST_SEND);
    }
}

void initBroadCast(char* filename){
    signal(SIGALRM,broadCastHandler);
    activeBroadcast = 1;
    broadCastFileName = filename;
    broadCastHandler(SIGALRM);
}

int handleUserInput(struct network_pair pair){
    char* token=strtok(get_input()," ");
    if(strcmp(token,UPLOAD_TOKEN) == 0 && activeServer){
        token = strtok(NULL,"\n");
        if(token==NULL || strtok(NULL," ")!=NULL){
            print_stdout(INVALID_INST);
            return 1;
        }
        if(!isFileExist(token)){
            print_stdout(INVALID_FILE_PATH);
            return 1;
        }
        sendToServer(pair,token);
        print_stdout(DATA_SEND_TO_SERVER);
        return 1;
    }
    else if(strcmp(token,DOWNLOAD_TOKEN) == 0){
        token = strtok(NULL,"\n");
        if(token==NULL || strtok(NULL," ")!=NULL){
            print_stdout(INVALID_INST);
            return 1;
        }
        if(activeServer){
            downloadReq(pair,token);
            print_stdout(DOWNLOAD_REQ_SEND);
        }
        else{
            initBroadCast(token);
        }
        return 1;
    }
    else{
        print_stdout(INVALID_INST);
        return 1;
    }
}

void getFromServer(struct network_pair pair){
    char* temp = malloc(1024);
    char* data = malloc(1);
    int rf;
    do{
        rf = recv(pair.socketfd,temp,1024,0);
        if(rf==0){
            print_stdout(SERVER_OFF);
            activeServer = 0;
            return;
        }
        data = realloc(data,strlen(temp)+strlen(data));
        data = strcat(data,temp);
    }while(rf==1024);
    char* filename = strtok(data," ");
    if(strcmp(filename,NO_FILE)==0){
        initBroadCast(serverReqFilename);
        serverReqFilename = NULL;
        print_stdout(SERVER_NO_FILE);
        return;
    }
    char* file = strtok(NULL,"\0");
    serverReqFilename = NULL;
    writeFile(filename,file,strlen(file));
    print_stdout(FIEL_SAVED);
}

int handleP2PBroadcastRecv(){
    char* data = (char*) malloc(INPUT_READ);
    int size = sizeof(*UDPPair.address);
    int status = recvfrom(UDPPair.socketfd,data,INPUT_READ,0,(struct sockaddr*)UDPPair.address,&size);
    UDPPair.address->sin_addr.s_addr = INADDR_BROADCAST;
    if(status==0){
        return -1;
    }
    char* filename = strtok(data," ");
    if(activeBroadcast && strcmp(filename,ANSWER_KEYWORD)==0){
        filename = strtok(NULL," ");
        char* port = strtok(NULL,"\0");
        if(strcmp(filename,broadCastFileName)==0){
            print_stdout(FILE_FIND_P2P);
            broadCastFileName = NULL;
            activeBroadcast = 0;
            struct network_pair pair = setTCPSocket(atoi(port),CLIENT);
            if(pair.socketfd<0){
                return -1;
            }
            alarm(0);
            downloadReq(pair,filename);
            return pair.socketfd;
        }
        return -1;
    }
    else if(strcmp(filename,ANSWER_KEYWORD)==0){
        return -1;
    } 
    char* port = strtok(NULL,"\0");
    if(strcmp(port_m,port)==0){
        return -1;
    }
    if(isFileExist(filename)==1){
        int len = strlen(ANSWER_KEYWORD)+1+strlen(filename) +1+ strlen(port_m)+1;
        char* data = (char*) malloc(len);
        data = strcat(data,ANSWER_KEYWORD);
        data = strcat(data," ");
        data = strcat(data,filename);
        data = strcat(data," ");
        data = strcat(data,port_m);
        data = strcat(data,"\0");
        sendto(UDPPair.socketfd,data,len,0,(struct sockaddr*)UDPPair.address,sizeof(*UDPPair.address));
        print_stdout(FILE_AVAILABLE_P2P);
        return -1;
    }
    if(!(broadCastFileName!=NULL && strcmp(filename,broadCastFileName)==0)){
        print_stdout(FILE_BROADCAST_NOT_FOUND);
    }
    return -1;
}

void communicate(struct network_pair pair){
    int run = 1,maxFd = -1;
    int sockNum = 4,i;
    int *sockets = (int*) malloc(sockNum*sizeof(int));
    sockets[0] = STDIN_FILENO;sockets[1] = pair.socketfd;sockets[2] = TCPPair.socketfd;sockets[3] = UDPPair.socketfd;
    fd_set socketSet;
    while(run){
        struct timeval time;
        time.tv_sec = 0;
        time.tv_usec = 50000;
        FD_ZERO(&socketSet);
        for(i=0;i<sockNum;i++){
            if(sockets[i]>=0){
                FD_SET(sockets[i],&socketSet);
            }
            if(maxFd<sockets[i]){
                maxFd = sockets[i];
            }
        }
        int result = select(maxFd+1,&socketSet,NULL,NULL,&time);
        if(result!=0 && result!=-1){
            if(FD_ISSET(sockets[0],&socketSet)){
                run = handleUserInput(pair);
            }
            else if(FD_ISSET(sockets[1],&socketSet) && activeServer){
                getFromServer(pair);
            }
            else if(FD_ISSET(sockets[2],&socketSet)){
                struct sockaddr addr;
                int size = sizeof(addr);
                int newfd = accept(sockets[2],&addr,&size);
                print_stdout(NEW_CLIENT_ADDED);
                sockets =addTovector(sockets,newfd,sockNum);
                sockNum++;
                if(newfd>maxFd){
                    maxFd = newfd;
                }
            }
            else if(FD_ISSET(sockets[3],&socketSet)){
                int status = handleP2PBroadcastRecv();
                if(status!=-1){
                    sockets =addTovector(sockets,status,sockNum);
                    sockNum++;
                    if(status>maxFd){
                        maxFd = status;
                    }
                }
            }
            for(i=4;i<sockNum;i++){
                if(FD_ISSET(sockets[i],&socketSet)){
                    handleClientReq(sockets[i]);
                    close(sockets[i]);
                    print_stdout(P2P_DIRECT_CONNECTION_CLOSED);
                    sockets = removeFromVector(sockets,sockets[i],sockNum);
                    sockNum--;
                }
            }
        }
    }
}

struct network_pair setUDPSocket(int port){
    int sockfd;
    int opt = 1;
    if((sockfd = socket(AF_INET,SOCK_DGRAM,0))<0){
        print_stdout(SOCK_ERR);
        _exit(EXIT_FAILURE);
    }
    struct sockaddr_in *addr = malloc(sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_BROADCAST;
    addr->sin_port = htons(port);
    struct timeval timeOut;
    timeOut.tv_usec = TIME_OUT_S*100000;
    timeOut.tv_sec = TIME_OUT_S;
    if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEPORT,&opt,sizeof(opt))<0){
        print_stdout(SOCK_SET_OPS_ERR);
        _exit(EXIT_FAILURE);
    }
    int opt2 = 1;
    if(setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&opt2,sizeof(opt2))<0){
        print_stdout(SOCK_SET_OPS_ERR);
        _exit(EXIT_FAILURE);
    }
    if(bind(sockfd,(struct sockaddr*)addr,sizeof(*addr))<0){
        print_stdout(SOCK_BIND_ERR);
        _exit(EXIT_FAILURE);
    }
    struct network_pair pair;
    pair.socketfd = sockfd;
    pair.address = addr;
    return pair;
}
struct network_pair setupTCPServerSocket(int listenPort){
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
    if(setsockopt(socketfd,SOL_SOCKET,SO_REUSEPORT,&optval,sizeof(optval))<0){
        print_stdout(SOCKET_SET_OPS_ERR);
        _exit(EXIT_FAILURE);
    }
    if(setsockopt(socketfd,SOL_SOCKET,SO_REUSEADDR,&optval2,sizeof(optval2))<0){
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

void init(int port_y,int port_m){
    UDPPair = setUDPSocket(port_y);
    TCPPair = setupTCPServerSocket(port_m);
}

int main(int argc,char** argv){
    if(argc!=4){
        print_stdout("invalid arguments.\nexit.\n");
        _exit(EXIT_FAILURE);
    }
    int port_x = atoi(argv[1]);
    port_m = argv[3];
    init(atoi(argv[2]),atoi(argv[3]));
    int serverPort=isServerExist(port_x);
    if(serverPort!=-1){
        activeServer = 1;
        struct network_pair pair = setTCPSocket(serverPort,SERVER);
        print_stdout(ACTIVE_SEREVR);
        communicate(pair);
        exit(EXIT_SUCCESS);
    }
    else{
        print_stdout(NOT_ACTIVE_SERVER);
        struct network_pair pair;
        pair.socketfd = -1;
        communicate(pair);
    }
}
