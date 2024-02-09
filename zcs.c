#include "zcs.h"
#include <stdio.h>
#include <pthread.h>
#include "multicast.h"



int isInit = 0; // 0 for false, 1 for true
char *LanIp ; 
int Nodetype;

mcast_t *AppM;
mcast_t *ServiceM;

LocalRegistry *LocalR;

pthread_mutex_t mutex = PTHREAD_COND_INITIALIZER;
pthread_t newThread;


int AddNode(LocalRegistry r){
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SERVICES; i++){
        if (LocalR != NULL && strcmp(LocalR[i].serviceName,r.serviceName) == 0){
            pthread_mutex_unlock(&mutex);
            freenode(r)
            return -1;           // Duplicate node name, reject
        }else if (LocalR == NULL){
            LocalR[i] == r;
            pthread_mutex_unlock(&mutex);
            return 0;           // Success
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1;                  // FULL
}

void freenode(LocalRegistry Node){
    int attrlen = Node.attr_num;
    for (int i = 0; i < attrlen; i++)
    {
        free(Node.AttributeList[i].attr_name);
        free(Node.AttributeList[i].value);
    }
    free(Node);
}


/*  Switch Node State to DOWN */
int goDown(char *name){
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SERVICES; i++){
        if (LocalR[i] != NULL && strcmp(name, LocalR[i].serviceName) == 0){
            LocalR[i].isAlive = 0;
            pthread_mutex_unlock(&mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&mutex);
    return 1;
}

/*  Switch Node State to UP */
int goUp(char *name){
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SERVICES; i++){
        if (LocalR[i] != NULL && strcmp(name, LocalR[i].serviceName) == 0){
            LocalR[i].isAlive = 1;
            pthread_mutex_unlock(&mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&mutex);
    return 1;
}


char* HeartBeatGenerate(char* ServiceName) {
    //HB#ServiceName
    char *HBMsg = (char *)malloc(2048);
    strcat(HBMsg, "HB#");
    strcat(HBMsg,ServiceName);
    return HBMsg;
    
}

char* AdvertisementGenerate(char* ServiceName) {
    //AD#ServiceName
    char *HBMsg = (char *)malloc(2048);
    strcat(HBMsg, "HB#");
    strcat(HBMsg,ServiceName);
    return HBMsg;
    
}

char* NotificationGenerate(char *ServiceName, zcs_attribute_t attr[], int num) {
    //"NOT#name#attrnum#attname,attval;..."
    char *NotMsg = (char *)malloc(2048);
    strcat(NotMsg, "NOT#");
    strcat(NotMsg,ServiceName);
    strcat(NotMsg,"#");
    char* numstring = (char *)malloc(2);
    sprintf(numstring, "%d", num);
    strcat(NotMsg,numstring);
    strcat(NotMsg,"#");
    for (int i = 0; i < num; i++)
    {
        char* Pair = (char *)malloc(75);
        strcat(Pair,attr[i].attr_name);
        strcat(Pair,",");
        strcat(Pair,attr[i].value);
        strcat(Pair,";");
        strcat(NotMsg,Pair);
        free(Pair)
    }
    
    str[strlen(NotMsg); - 1] = '\0';
    return NotMsg;
    
}

void SendMsg(mcast_t Destination, char* msg) {
    multicast_send(Destination, msg, strlen(msg));
    return ;
}

LocalRegistry NotificationDecode(char *NotMsg) {
    //"name#attname,attval;..."
    LocalRegistry Newnode = malloc(sizeof(LocalRegistry));
    //char name[64];
    char* buffer = (char *)malloc(100);
    buffer = strtok(NotMsg, "#");
    strcpy(Newnode.serviceName,buffer);
    buffer = strtok(NotMsg, "#");
    int num = atoi(buffer);
    Newnode.attr_num = num;
    Newnode.isAlive=1;
    //add node
    buffer = strtok(NotMsg, ";");
    int i=0;
    while (buffer != NULL)
    {
        
        char* attrname = (char *)malloc(40);
        char* attrval = (char *)malloc(30);
        strcpy(attrname,strtok(buffer, ","));
        strcpy(attrval,buffer);
        Newnode.AttributeList[i].attr_name=attrname;
        Newnode.AttributeList[i].value=attrval;
        i++;
    }
    
    free(NotMsg);
    return Newnode;
    
}

void *AppListenThread() {
    // in App
    while(1) {
        //receive
        char msg [100];
        multicast_setup_recv(ServiceM);
        while (multicast_check_receive(ServiceM) == 0) {
	    multicast_send(ServiceM, msg, strlen(msg));
        msg is HB?

	    printf("repeat.. \n");
        }
    }
    return ;
}

void *ServiceListenThread(){

}





char* getIP(){
    char hostname[1024];
    struct hostent *host_entry;
    char *ip;

    // Get the hostname
    if (gethostname(hostname, sizeof(hostname)) == -1) {
        perror("gethostname");
        return 1;
    }

    // Get hostent structure for the hostname
    if ((host_entry = gethostbyname(hostname)) == NULL) {
        perror("gethostbyname");
        return 1;
    }

    // Convert the IP address to a string
    ip = inet_ntoa(*(struct in_addr *)host_entry->h_addr_list[0]);

    printf("Hostname: %s\n", hostname);
    printf("IP Address: %s\n", ip);

    return ip;
}

int zcs_init(int type , char *MulticastConfig){
    //MulticastConfig = "ip#sport#rport"

    AppM = multicast_init(LanIp, APPRPORT, APPSPORT);
    ServiceM = multicast_init(LanIp, SERVICERPORT, SERVICESPORT);
    Nodetype = type;
    if (type == ZCS_APP_TYPE)
    {
        LocalR = malloc(MAX_SERVICES*sizeof(LocalRegistry));
        isInit = 1;
        //Add listenner
    }
    else if (type == ZCS_SERVICE_TYPE)
    {
        isInit = 1;
        //Add beater and listenner
    }
    else{
        
    }
    return -1;
    
};

int zcs_start(char *name, zcs_attribute_t attr[], int num){

    
    if(Nodetype == 1){      // APP
        DiscoveryGenerate();
        pthread_create(&newThread, NULL, AppListenThread, NULL); 
    }else{                  // Service
        pthread_create(&newThread, NULL, ServiceListenThread, NULL); 
    }
    pthread_create(&heartbeatThread, NULL, HeartBeatGenerate,NULL);
}


int zcs_post_ad(char *ad_name, char *ad_value){
    //post ad name and val 
};

int zcs_query(char *attr_name, char *attr_value, char *node_names[]){
    int count = 0;
    for (int i = 0; i < MAX_SERVICES; i++){
        if (LocalR[i] != NULL ){
            for (int j = 0; j < MAX_SERVICE_ATTRIBUTE; j++){
                if (strcmp(attr_name,LocalR[i].AttributeList[j].attr_name) == 0 && 
                    strcmp(attr_value, LocalR[i].AttributeList[j].value) == 0){
                        node_names[count++] = LocalR[i].serviceName;
                    }
            }
        }
    }
    return 0;

};

int zcs_get_attribs(char *name, zcs_attribute_t attr[], int *num){
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SERVICES; i++){
        if (LocalR[i] != NULL && strcmp(LocalR[i].serviceName,name) == 0){
            for (int count = 0; count < num; count++){
                zcs_attribute_t copy;
                copy.attr_name = LocalR[i].AttributeList[count].attr_name;
                copy.attr_name = LocalR[i].AttributeList[count].value;
                attr[count] = copy;
                pthread_mutex_unlock(&mutex);
                return 0;
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1;
};

int zcs_listen_ad(char *name, zcs_cb_f cback){};

int zcs_shutdown(){};

void zcs_log(){
    printf("====== Log ======\n");
    for (int i = 0; i < MAX_SERVICES; i++){
        if (LocalR[i] != NULL){
            printf("Name: %s, State: %s, Attributes: ",LocalR[i].serviceName, LocalR[i].isAlive);
            for (int j = 0; j < MAX_SERVICE_ATTRIBUTE; j++){        // print all attributes of current node
                if (LocalR[i].AttributeList[j] == NULL){
                    printf("\n");
                    break;
                }
                printf("(%s,%s), ",LocalR[i].AttributeList[j].attr_name,LocalR[i].AttributeList[j].value);
            }
        }
    }
};