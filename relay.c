#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "multicast.h"


#define MAX_RALEY_WIDTH 2               // Max number of nodes in local registry
#define MAX_SERVICE_ATTRIBUTE 10    // Max number of attributes of a node
#define MAX_CALLBACK_NUM 10         // Max CallBack function that a node can register
#define MAX_MSG_LENGTH 1024        
#define MAX_NODE_NAME_LENGTH 64 

#define APP_SEND_PORT   4096
#define SERVICE_SEND_PORT 5024
#define DEFAULT_PORT 4004

#define TIMEOUT 1

typedef struct {
    pthread_t threads[2];
    //in order 
    // App_transit_thread
    // Service_transit_thread
} ThreadEntry;
typedef struct {
    mcast_t * sockets[4];
    //in order 
    // App_Service_Send 
    // App_Service_Receive
    // Service_App_Send
    // Service_App_Receive

} SocketEntry;

typedef struct {
    ThreadEntry listners;
    ThreadEntry sockets;
} RelayEntry;

RelayEntry RelayTable[MAX_RALEY_WIDTH];


char *ip1A = "224.1.1.1";
char *ip1S = "224.1.1.2";

char *ip2A = "224.1.2.1";
char *ip2S = "224.1.2.2";

mcast_t *Lan1_App_Service_Receive;
mcast_t *Lan1_Service_App_Receive;
mcast_t *Lan2_App_Service_Receive;
mcast_t *Lan2_Service_App_Receive;


mcast_t *Lan1_App_Service_Send;
mcast_t *Lan1_Service_App_Send;
mcast_t *Lan2_App_Service_Send;
mcast_t *Lan2_Service_App_Send;

int join_threads = 0;               // 0 for false, 1 for true
pthread_t Lan1AppListenerThread;           
pthread_t Lan1ServiceListenerThread;
pthread_t Lan2AppListenerThread;           
pthread_t Lan2ServiceListenerThread;

char *discovery = "D";

/* Send msg to channel 'wave' times. */
void send_msg(mcast_t *channel, void *msg, int msglen, int wave){
    for (int i = 0; i < wave; i++){
        int err = multicast_send(channel,msg,msglen);
        //printf("Message Send: %d\n",err);
        usleep(10000);
    }
}

void *TransitThread(void *arg){
    //arg/2 is index 
    //arg//2 : if 0 APP if 1 Server listener
    int index;
    int IsApp;
    if (IsApp == 0)
    {
        multicast_setup_recv(Lan1_App_Service_Receive);
        char msg[MAX_MSG_LENGTH];
        while(join_threads == 0){
        //printf("ServiceListener is alive...\n");
        memset(msg,'\0',sizeof(msg));
        while(multicast_check_receive(Lan1_App_Service_Receive) == 0){
            //printf("Service is waiting for message...\n");
        }
        multicast_receive(Lan1_App_Service_Receive,msg,MAX_MSG_LENGTH);
        printf("App's new msg: %s\n",msg);
        
        send_msg(Lan2_Service_App_Send,msg,MAX_MSG_LENGTH,1);
        
    }
    }
    else
    {
        multicast_setup_recv(Lan1_App_Service_Receive);
        char msg[MAX_MSG_LENGTH];
        while(join_threads == 0){
        //printf("ServiceListener is alive...\n");
        memset(msg,'\0',sizeof(msg));
        while(multicast_check_receive(Lan1_App_Service_Receive) == 0){
            //printf("Service is waiting for message...\n");
        }
        multicast_receive(Lan1_App_Service_Receive,msg,MAX_MSG_LENGTH);
        printf("App's new msg: %s\n",msg);
        
        send_msg(Lan2_Service_App_Send,msg,MAX_MSG_LENGTH,1);
    }
    }
    
    
}




void *Lan1AppListener(){
    multicast_setup_recv(Lan1_App_Service_Receive);
    char msg[MAX_MSG_LENGTH];
    while(join_threads == 0){
        //printf("ServiceListener is alive...\n");
        memset(msg,'\0',sizeof(msg));
        while(multicast_check_receive(Lan1_App_Service_Receive) == 0){
            //printf("Service is waiting for message...\n");
        }
        multicast_receive(Lan1_App_Service_Receive,msg,MAX_MSG_LENGTH);
        printf("App's new msg: %s\n",msg);
        
        send_msg(Lan2_Service_App_Send,msg,MAX_MSG_LENGTH,1);
        
    }
}

void *Lan1ServiceListener(){
    multicast_setup_recv(Lan1_Service_App_Receive);
    char msg[MAX_MSG_LENGTH];
    while(join_threads == 0){
        //printf("ServiceListener is alive...\n");
        memset(msg,'\0',sizeof(msg));
        while(multicast_check_receive(Lan1_Service_App_Receive) == 0){
            //printf("Service is waiting for message...\n");
        }
        multicast_receive(Lan1_Service_App_Receive,msg,MAX_MSG_LENGTH);
        printf("App's new msg: %s\n",msg);
        
        send_msg(Lan2_Service_App_Send,msg,strlen(msg),1);
    }
}

void *Lan2AppListener(){
    multicast_setup_recv(Lan2_App_Service_Receive);
    char msg[MAX_MSG_LENGTH];
    while(join_threads == 0){
        //printf("ServiceListener is alive...\n");
        memset(msg,'\0',sizeof(msg));
        while(multicast_check_receive(Lan2_App_Service_Receive) == 0){
            //printf("Service is waiting for message...\n");
        }
        multicast_receive(Lan2_App_Service_Receive,msg,MAX_MSG_LENGTH);
        printf("App's new msg: %s\n",msg);
        
        send_msg(Lan1_Service_App_Send,msg,MAX_MSG_LENGTH,1);
        
    }
}

void *Lan2ServiceListener(){
    multicast_setup_recv(Lan2_Service_App_Receive);
    char msg[MAX_MSG_LENGTH];
    while(join_threads == 0){
        //printf("ServiceListener is alive...\n");
        memset(msg,'\0',sizeof(msg));
        while(multicast_check_receive(Lan2_Service_App_Receive) == 0){
            //printf("Service is waiting for message...\n");
        }
        multicast_receive(Lan2_Service_App_Receive,msg,MAX_MSG_LENGTH);
        printf("App's new msg: %s\n",msg);
        
        send_msg(Lan1_Service_App_Send,msg,strlen(msg),1);
    }
}



int relay_init(){
    //Lan 1 channel
    Lan1_App_Service_Receive = multicast_init(ip1S,DEFAULT_PORT,SERVICE_SEND_PORT);
    Lan1_App_Service_Send = multicast_init(ip1A,APP_SEND_PORT,DEFAULT_PORT);
    Lan1_Service_App_Receive = multicast_init(ip1A,DEFAULT_PORT,APP_SEND_PORT);
    Lan1_Service_App_Send = multicast_init(ip1S,SERVICE_SEND_PORT,DEFAULT_PORT);

    //Lan 2 channel
    Lan2_App_Service_Receive = multicast_init(ip2S,DEFAULT_PORT,SERVICE_SEND_PORT);
    Lan2_App_Service_Send = multicast_init(ip2S,APP_SEND_PORT,DEFAULT_PORT);
    Lan2_Service_App_Receive = multicast_init(ip2S,DEFAULT_PORT,APP_SEND_PORT);
    Lan2_Service_App_Send = multicast_init(ip2S,SERVICE_SEND_PORT,DEFAULT_PORT);

    return 0;
}

void SetupSockets(int index,char* ips[],int AppS,int AppR , int ServS, int ServR){
    //ips len 2 appip,serverIp
    RelayTable[index].sockets[0]=multicast_init(ips[0],APP_SEND_PORT,DEFAULT_PORT);
    RelayTable[index].sockets[1]=multicast_init(ips[0],DEFAULT_PORT,SERVICE_SEND_PORT);
    RelayTable[i].sockets[2]=multicast_init(ips[1],SERVICE_SEND_PORT,DEFAULT_PORT);
    RelayTable[i].sockets[3]=multicast_init(ips[1],DEFAULT_PORT,APP_SEND_PORT);
    
}

void StartThread(int index){
    //ips len 2 appip,serverIp
    pthread_create(&TransitThread,NULL,Lan1AppListener,NULL);
    pthread_create(&TransitThread,NULL,Lan1ServiceListener,NULL);
    send_msg(Lan1_Service_App_Send,discovery,strlen(discovery),3);
}

int relay_up(){
    // for (int i = 0; i < 2; i++)
    // {
    
    //     pthread_create(&ListenerThread,NULL,AppListener,NULL);
    //     pthread_create(&ListenerThread,NULL,AppListener,NULL);
    // }
    
    pthread_create(&Lan1AppListenerThread,NULL,Lan1AppListener,NULL);
    pthread_create(&Lan1ServiceListenerThread,NULL,Lan1ServiceListener,NULL);
    pthread_create(&Lan2AppListenerThread,NULL,Lan2AppListener,NULL);
    pthread_create(&Lan2ServiceListenerThread,NULL,Lan2ServiceListener,NULL);

    send_msg(Lan1_Service_App_Send,discovery,strlen(discovery),3);
    send_msg(Lan2_Service_App_Send,discovery,strlen(discovery),3);
    return 0;
}


int relay_down(int type){
    join_threads=1;
    return 0;
}



int main(){
    //input?

    int a = relay_init();
    a=relay_up();
    sleep(300);
    a=relay_down();
    return a;
}