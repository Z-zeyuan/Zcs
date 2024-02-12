#include "zcs.h"
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include "multicast.h"



int AD_Post_Num 5;
int AD_Send_Interval 0.1;

int isInit = 0; // 0 for false, 1 for true
char *LanIp ; 
int Nodetype;
LocalRegistry *thisNode == NULL;


mcast_t *AppM;
mcast_t *ServiceM;

LocalRegistry *LocalR;

pthread_mutex_t mutex = PTHREAD_COND_INITIALIZER;
pthread_t ListenerThread;
pthread_t HeartBeatGenerateThread;

int AddNode(LocalRegistry r){
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SERVICES; i++){
        if (LocalR != NULL && strcmp(LocalR[i].serviceName,r.serviceName) == 0){
            pthread_mutex_unlock(&mutex);
            return -1;           // Duplicate node name, reject
        }else if (LocalR == NULL){
            LocalR[i] = r;
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


char* HeartBeatGenerate(char ServiceName[]) {
    //HB#ServiceName
    char *HBMsg = (char *)malloc(MAX_MSG_Size);
    strcat(HBMsg, "HB#");
    strcat(HBMsg,ServiceName);
    return HBMsg;
    
}

char* AdvertisementGenerate(char* AdName, char* AdVal) {
    //AD#ServiceName
    char *ADMsg = (char *)malloc(MAX_MSG_Size);
    strcat(ADMsg, "AD#");
    strcat(ADMsg,AdName);
    strcat(ADMsg,";");
    strcat(ADMsg,AdVal);
    return ADMsg;
    
}

char* NotificationGenerate(char *ServiceName, zcs_attribute_t attr[], int num) {
    //"NOT#name#attrnum#attname,attval;..."
    char *NotMsg = (char *)malloc(MAX_MSG_Size);
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
    
    NotMsg[strlen(NotMsg); - 1] = '\0';
    return NotMsg;
    
}

void SendMsg(mcast_t Destination, char* msg) {
    multicast_send(Destination, msg, strlen(msg));
    return ;
}

LocalRegistry NotificationDecode(char *NotMsg) {
    //"name#attname,attval;..."
    LocalRegistry Newnode =(LocalRegistry *)malloc(sizeof(LocalRegistry));
    //char name[64];
    char *NotMsg_copy = (char *)malloc(MAX_MSG_Size);
    strcpy(NotMsg_copy,NotMsg);
    char* buffer = (char *)malloc(100);
    buffer = strtok_r(NotMsg_copy, "#",&NotMsg_copy);

    buffer[strlen(buffer) - 1] = '\0';
    NotMsg_copy[strlen(NotMsg_copy) - 1] = '\0';

    strcpy(Newnode.serviceName,buffer);
    buffer = strtok_r(NotMsg_copy, "#",&NotMsg_copy);
    
    buffer[strlen(buffer) - 1] = '\0';
    NotMsg_copy[strlen(NotMsg_copy) - 1] = '\0';

    int num = atoi(buffer);
    Newnode.attr_num = num;
    Newnode.isAlive=1;
    //add node
    buffer = strtok_r(NotMsg_copy, ";",&NotMsg_copy);
    buffer[strlen(buffer) - 1] = '\0';
    NotMsg_copy[strlen(NotMsg_copy) - 1] = '\0';

    int i=0;
    while (buffer != NULL)
    {
        
        char* attrname = (char *)malloc(40);
        char* attrval = (char *)malloc(30);
        strcpy(attrname,strtok_r(buffer, ",",&buffer));
        strcpy(attrval,buffer);
        
        attrname[strlen(attrname) - 1] = '\0';
        attrval[strlen(attrval) - 1] = '\0';

        Newnode.AttributeList[i].attr_name=attrname;
        Newnode.AttributeList[i].value=attrval;
        
        buffer = strtok_r(NotMsg_copy, ";",&NotMsg_copy);
        buffer[strlen(buffer) - 1] = '\0';
        NotMsg_copy[strlen(NotMsg_copy) - 1] = '\0';    
        i++;
    }
    
    free(NotMsg);
    return Newnode;
    
}



int messageType(char *msg){
    char* HeadLable = strtok_r(msg, "#",&msg);
    HeadLable[strlen(HeadLable) - 1] = '\0';
    msg[strlen(msg) - 1] = '\0';
    if (strcmp(HeadLable,"HB")){
        return 1;
    }
    else if (strcmp(HeadLable,"NOT")){
        return 2;
    }
    else if (strcmp(HeadLable,"AD")){
        return 3;
    }
    return 99;
    
}

void HeartbeatCount(dict *d, char *name){
    for (int i = 0; i < MAX_MSG_Size; i++){
        if (d[i] != NULL && strcmp(d[i].name, name) == 0){
            d[i].count++;
        }else if(d[i] == NULL){
            d[i].name = name;
            d[i].count = 0;
        }
    }
}

void updateThreadTable(dict *d){
    pthread_mutex_unlock(&mutex);
    for(int i = 0; i < MAX_SERVICES; i++){
        if (d[i] == NULL) {
            pthread_mutex_unlock(&mutex);
            return;
        }
        else{
            char *node_name = d[i].name;
            int isAlive = (d[i].count >= 3) ? 1 : 0;
            for (int j = 0; j < MAX_SERVICES; j++){
                if (strcmp(LocalR[j].serviceName,node_name) == 0){
                    LocalR[j].isAlive = isAlive;
                }
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return;
}

void *AppListenThread() {
    dict *thread_table = (dict *)malloc(MAX_SERVICES * sizeof(dict));
    time_t start_time;
    double elapsed_time;
    int restart_time = 1;
    // in App
    while(1) {
        if (restart_time == 1){
            restart_time = 0;
            start_time = time(NULL);
        }
        //receive
        char msg [MAX_MSG_Size];
        multicast_setup_recv(ServiceM);
        while (multicast_check_receive(ServiceM) == 0) {   // Check if there's new msg
            // Spin
            if (difftime(time(NULL), start_time) >= TIMEOUT){
            updateThreadTable(thread_table);
            restart_time = 1;
            }   
        }
        multicast_receive(ServiceM,msg,MAX_MSG_Size);
        
        int msgtype = messageType(msg);

        switch (msgtype)
        {
        case 1:         // Heartbeat
            char node_name[MAX_NODE_NAME_SIZE] = HeartBeatDecode(msg);
            HeartbeatCount(thread_table,node_name);
            break;
        case 2:         // Notification

            LocalRegistry node = NotificationDecode(msg);
            int errCode = AddNode(node);
            
            if (errCode == -1){
                freenode(node);
            }   // Otherwise continue

            HeartbeatCount(thread_table,node.serviceName);
            break;
        default:
            break;
        }
        if (difftime(time(NULL), start_time) >= TIMEOUT){
            updateThreadTable(thread_table);
            restart_time = 1;
        }
    }
    return ;
}

void *ServiceListenThread(){
    while(1){
        char* msg = (char *) malloc(MAX_MSG_Size);
        multicast_setup_recv(AppM);
        while (multicast_check_receive(ServiceM) == 0) {
            // Spin
            if (difftime(time(NULL), start_time) >= TIMEOUT){
            restart_time = 1;
        }
        }
        multicast_receive(AppM,msg,MAX_MSG_Size)

    }
}


void *HBSenderThread() {
    // in App
    while(1) {
        sleep(0.01);
        char* HBmsg = HeartBeatGenerate(thisNode->serviceName);
        SendMsg(AppM,HBmsg);
    }
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
    if(AppM == NULL || ServiceM == NULL ){
        return -1;
    }
    if (type != ZCS_APP_TYPE && type != ZCS_SERVICE_TYPE )
    {
        return -1;
    }    
    Nodetype = type;
    if (type == ZCS_APP_TYPE)
    {
        LocalR = (LocalRegistry *)malloc(MAX_SERVICES*sizeof(LocalRegistry));
        if(LocalR == NULL){return -1;}
        isInit = 1;
    }
    else if (type == ZCS_SERVICE_TYPE)
    {
        isInit = 1;
    }
    return 0;
};

LocalRegistry *initializeNode(const char *name, const zcs_attribute_t attr[], int num) {
    // Allocate memory for Node
    LocalRegistry *newNode = (LocalRegistry *)malloc(sizeof(LocalRegistry));
    if (newNode == NULL) {
        // Error handling for memory allocation failure
        fprintf(stderr, "Memory allocation failed for LocalRegistry\n");
        return NULL;
    }

    // Copy serviceName
    strncpy(newNode->serviceName, name, MAX_NODE_NAME_SIZE - 1);
    newNode->serviceName[MAX_NODE_NAME_SIZE - 1] = '\0';

    newNode->attr_num = num;

    newNode->isAliveTimeCount = 0;
    newNode->isAlive = 0;

    // Copy AttributeList
    for (int i = 0; i < num; i++) {
        newNode->AttributeList[i].attr_name = strdup(attr[i].attr_name);
        newNode->AttributeList[i].value = strdup(attr[i].value);
    }

    return newNode;
}

int zcs_start(char *name, zcs_attribute_t attr[], int num){
    if(isInit == 0){return -1;}
    thisNode = initializeNode(name,attr,num);
    if(Nodetype == ZCS_APP_TYPE){      // APP
        pthread_create(&ListenerThread, NULL, AppListenThread, NULL); 
        DiscoveryGenerate();
    }else{                  // Service
        pthread_create(&ListenerThread, NULL, ServiceListenThread, NULL); 
        pthread_create(&HeartBeatGenerateThread, NULL, HBSenderThread,NULL);
    }
    
    return 0;
    
}


int zcs_post_ad(char *ad_name, char *ad_value){
    if(thisNode == NULL){
        return 0;
    }
    char *ADMessage = AdvertisementGenerate(ad_name,ad_value);
    int SendCount=0;
    for (int i = 0; i < AD_Post_Num; i++)
    {

        SendCount++;
    }
    

    return AD_Post_Num;
};

int zcs_query(char *attr_name, char *attr_value, char *node_names[]){
    int count = 0;
    for (int i = 0; i < MAX_SERVICES; i++){
        if (LocalR[i] != NULL ){
            for (int j = 0; j < MAX_SERVICE_ATTRIBUTE; j++){
                if (strcmp(attr_name,LocalR[i].AttributeList[j].attr_name) == 0 && 
                    strcmp(attr_value, LocalR[i].AttributeList[j].value) == 0){
                        strcpy(node_names[count++] , LocalR[i].serviceName);
                    }
            }
        }
    }
    return count;
};

int zcs_get_attribs(char *name, zcs_attribute_t attr[], int *num){
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SERVICES; i++){
        if (LocalR[i] != NULL && strcmp(LocalR[i].serviceName,name) == 0){
            for (int count = 0; count < num; count++){
                strcpy(attr[count].attr_name , LocalR[i].AttributeList[count].attr_name);
                strcpy(attr[count].attr_name , LocalR[i].AttributeList[count].value);
                pthread_mutex_unlock(&mutex);
                return 0;
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1;
};

int zcs_listen_ad(char *name, zcs_cb_f cback){};

int zcs_shutdown(){
    if (Nodetype == ZCS_SERVICE_TYPE)
    {
        //join all threads
    }
    else
    {
        //join all threads
    }
    
    
};

void zcs_log(){
    printf("====== Log ======\n");
    for (int i = 0; i < MAX_SERVICES; i++){
        if (LocalR[i] != NULL){
            printf("Name: %s, State: %s, Attributes: ",LocalR[i].serviceName, LocalR[i].isAlive);
            for (int j = 0; j < LocalR[i].attr_num; j++){        // print all attributes of current node
                if (LocalR[i].AttributeList[j] == NULL){
                    printf("\n");
                    break;
                }
                printf("(%s,%s), ",LocalR[i].AttributeList[j].attr_name,LocalR[i].AttributeList[j].value);
            }
        }
    }
};