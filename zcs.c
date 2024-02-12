#include "zcs.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include "multicast.h"

int AD_Post_Num = 5;
//micro sec
int AD_Send_Interval = 100000;

int isInit = 0;                     // 0 for false, 1 for true
char *LanIp;
int Nodetype;
int join_threads = 0;               // 0 for false, 1 for true
LocalRegistry *thisNode = NULL;

mcast_t *AppM;                      
mcast_t *ServiceM;

LocalRegistry *LocalR;              // Pointer of local registry table

AdCallbackListenDict *AdListenDict = NULL;
pthread_mutex_t mutex = PTHREAD_COND_INITIALIZER;
pthread_t ListenerThread;           
pthread_t HeartBeatGenerateThread;


/* Add a node to the local registry. Do nothing and return -1 if node exists. 
    Return 0 if success. Return -2 if no available space.*/
int AddNode(LocalRegistry r)
{
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SERVICES; i++)
    {
        if (LocalR != NULL && strcmp(LocalR[i].serviceName, r.serviceName) == 0)
        {
            pthread_mutex_unlock(&mutex);
            return -1; // Duplicate node name, reject
        }
        else if (LocalR == NULL)
        {
            LocalR[i] = r;
            pthread_mutex_unlock(&mutex);
            return 0; // Success
        }
    }
    pthread_mutex_unlock(&mutex);
    return -2; // FULL
}

void freenode(LocalRegistry *Node)
{
    int attrlen = Node->attr_num;
    for (int i = 0; i < attrlen; i++)
    {
        free(Node->AttributeList[i].attr_name);
        free(Node->AttributeList[i].value);
    }
    free(Node);
}

/*  Generate and return a HEARTBEAT message for the given node name. */
char *HeartBeatGenerate(char ServiceName[])
{
    // HB#ServiceName
    char *HBMsg = (char *)malloc(MAX_MSG_Size);
    strcat(HBMsg, "HB#");
    strcat(HBMsg, ServiceName);
    return HBMsg;
}

/* Generate and return an advertisement message. */
char *AdvertisementGenerate(char *AdName, char *AdVal)
{
    // AD#ServiceName#Adname;Adval
    char *ADMsg = (char *)malloc(MAX_MSG_Size);
    strcat(ADMsg, "AD#");
    strcat(ADMsg, thisNode->serviceName);
    strcat(ADMsg, "#");
    strcat(ADMsg, AdName);
    strcat(ADMsg, ";");
    strcat(ADMsg, AdVal);
    return ADMsg;
}

/* Generate a notification message.  */
char *NotificationGenerate(char *ServiceName, zcs_attribute_t attr[], int num)
{
    //"NOT#name#attrnum#attname,attval;..."
    char *NotMsg = (char *)malloc(MAX_MSG_Size);
    strcat(NotMsg, "NOT#");
    strcat(NotMsg, ServiceName);
    strcat(NotMsg, "#");
    char *numstring = (char *)malloc(2);
    sprintf(numstring, "%d", num);
    strcat(NotMsg, numstring);
    strcat(NotMsg, "#");
    for (int i = 0; i < num; i++)
    {
        char *Pair = (char *)malloc(75);
        strcat(Pair, attr[i].attr_name);
        strcat(Pair, ",");
        strcat(Pair, attr[i].value);
        strcat(Pair, ";");
        strcat(NotMsg, Pair);
        free(Pair);
    }

    NotMsg[strlen(NotMsg) -1] = '\0';
    return NotMsg;
}

/* Send the message to the given multicast channel. */
void SendMsg(mcast_t *Destination, char *msg)
{
    multicast_send(Destination, msg, strlen(msg));
    return;
}

/* Send 'WaveSize' number of messages in 'interval' time to the given multicast channel*/
int SendWaveMsg(mcast_t *Destination, char *msg, int interval, int WaveSize)
{
    int count = 0;
    for (int i = 0; i < WaveSize; i++)
    {
        
        usleep(interval);
        SendMsg(Destination, msg);
        count++;
    }
    free(msg);
    return count;
}

/* Decode the given notification message, then generate and return a node */
LocalRegistry* NotificationDecode(char *NotMsg)
{
    //"name#attname,attval;..."
    LocalRegistry* Newnode = (LocalRegistry *)malloc(sizeof(LocalRegistry));
    // char name[64];
    char *NotMsg_copy = (char *)malloc(MAX_MSG_Size);
    strcpy(NotMsg_copy, NotMsg);
    char *buffer = (char *)malloc(100);
    buffer = strtok_r(NotMsg_copy, "#", &NotMsg_copy);

    buffer[strlen(buffer) - 1] = '\0';
    NotMsg_copy[strlen(NotMsg_copy) - 1] = '\0';

    strcpy(Newnode->serviceName, buffer);
    buffer = strtok_r(NotMsg_copy, "#", &NotMsg_copy);

    buffer[strlen(buffer) - 1] = '\0';
    NotMsg_copy[strlen(NotMsg_copy) - 1] = '\0';

    int num = atoi(buffer);
    Newnode->attr_num = num;
    Newnode->isAlive = 1;
    // add node
    buffer = strtok_r(NotMsg_copy, ";", &NotMsg_copy);
    buffer[strlen(buffer) - 1] = '\0';
    NotMsg_copy[strlen(NotMsg_copy) - 1] = '\0';

    int i = 0;
    while (buffer != NULL)
    {

        char *attrname = (char *)malloc(40);
        char *attrval = (char *)malloc(30);
        strcpy(attrname, strtok_r(buffer, ",", &buffer));
        strcpy(attrval, buffer);

        attrname[strlen(attrname) - 1] = '\0';
        attrval[strlen(attrval) - 1] = '\0';

        Newnode->AttributeList[i].attr_name = attrname;
        Newnode->AttributeList[i].value = attrval;

        buffer = strtok_r(NotMsg_copy, ";", &NotMsg_copy);
        buffer[strlen(buffer) - 1] = '\0';
        NotMsg_copy[strlen(NotMsg_copy) - 1] = '\0';
        i++;
    }

    free(NotMsg);
    free(buffer);
    return Newnode;
}

/* Return an integer that represents the message type of the given message.
    Return 1 if it's HEARTBEAT
    Return 2 if it's Nofitication
    Return 3 if it's Advertisement
    Return 20 if it's Discovery
    Return 99 otherwise. */
int messageType(char *msg)
{
    char *HeadLable = strtok_r(msg, "#", &msg);
    HeadLable[strlen(HeadLable) - 1] = '\0';
    msg[strlen(msg) - 1] = '\0';
    if (strcmp(HeadLable, "HB") == 0)
    {
        return 1;
    }
    else if (strcmp(HeadLable, "NOT") == 0)
    {
        return 2;
    }
    else if (strcmp(HeadLable, "AD") == 0)
    {
        return 3;
    }
    else if (strcmp(HeadLable, "DISCOVERY") == 0)
    {
        return 20;
    }
    return 99;
}

void HeartbeatCount(dict *d, char *name)
{
    for (int i = 0; i < MAX_MSG_Size; i++)
    {
        if (d[i].name != NULL && strcmp(d[i].name, name) == 0)
        {
            d[i].count++;
        }
        else if (d[i].name == NULL)
        {
            d[i].name = name;
            d[i].count = 1;
        }
    }
}

void updateThreadTable(dict *d)
{
    pthread_mutex_unlock(&mutex);
    for (int i = 0; i < MAX_SERVICES; i++)
    {
        if (d[i].name == NULL)
        {
            pthread_mutex_unlock(&mutex);
            return;
        }
        else
        {
            char *node_name = d[i].name;
            int isAlive = (d[i].count >= 3) ? 1 : 0;
            for (int j = 0; j < MAX_SERVICES; j++)
            {
                if (strcmp(LocalR[j].serviceName, node_name) == 0)
                {
                    LocalR[j].isAlive = isAlive;
                }
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return;
}

/* Thread function that listens all message for app. 
    Once a msg is received, it decodes the msg and do different 
    work, based on the type and the body of the msg. 
    Thread terminates if zcs_shundown() is called. */
void *AppListenThread()
{
    dict *thread_table = (dict *)malloc(MAX_SERVICES * sizeof(dict));
    time_t start_time;
    double elapsed_time;
    int restart_time = 1;
    // in App
    while (join_threads == 0)
    {
        if (restart_time == 1)
        {
            restart_time = 0;
            start_time = time(NULL);
        }
        // receive
        char msg[MAX_MSG_Size];
        multicast_setup_recv(ServiceM);
        while (multicast_check_receive(ServiceM) == 0)
        { // Check if there's new msg
            // Spin
            if (difftime(time(NULL), start_time) >= TIMEOUT)
            {
                updateThreadTable(thread_table);
                restart_time = 1;
            }
        }
        multicast_receive(ServiceM, msg, MAX_MSG_Size);

        int msgtype = messageType(msg);

        switch (msgtype)
        {
        case 1: // Heartbeat
            char node_name[MAX_NODE_NAME_SIZE];
            strcpy(node_name,msg);
            HeartbeatCount(thread_table, node_name);
            break;
        case 2: // Notification

            LocalRegistry* node = NotificationDecode(msg);
            int errCode = AddNode(*node);

            if (errCode == -1)
            {
                freenode(node);
                break;
            } // Otherwise continue

            HeartbeatCount(thread_table, node->serviceName);
            break;
        case 3: // Advertisement
            char *ADMsg_copy = (char *)malloc(MAX_MSG_Size);
            strcpy(ADMsg_copy, msg);
            char *buffer = (char *)malloc(100);
            buffer = strtok_r(ADMsg_copy, "#", &ADMsg_copy);
            buffer[strlen(buffer) - 1] = '\0';
            ADMsg_copy[strlen(ADMsg_copy) - 1] = '\0';


            for (int i = 0; i < MAX_MSG_Size; i++)
            {
                if (AdListenDict[i].SName != NULL && strcmp(AdListenDict[i].SName, buffer) == 0)
                {
                    char *AdName = (char *)malloc(200);
                    AdName = strtok_r(ADMsg_copy, ";", &ADMsg_copy);
                    AdName[strlen(buffer) - 1] = '\0';
                    ADMsg_copy[strlen(ADMsg_copy) - 1] = '\0';
                    char* AdVal = ADMsg_copy;
                    AdListenDict->callback(AdName,AdVal);
                }
                else if (AdListenDict[i].SName == NULL)
                {break;}
            }


        default:
            break;
        }
        if (difftime(time(NULL), start_time) >= TIMEOUT)
        {
            updateThreadTable(thread_table);
            restart_time = 1;
        }
    }
    return;
}

/* Thread function that listens all message for service. 
    Once a msg is received, it decodes the msg and do different 
    work, based on the type and the body of the msg. 
    Thread terminates if zcs_shundown() is called. */
void *ServiceListenThread()
{
    // Service
    while (join_threads == 0)
    {

        // receive
        char msg[MAX_MSG_Size];
        multicast_setup_recv(ServiceM);
        while (multicast_check_receive(ServiceM) == 0)
        { // Check if there's new msg
        }
        multicast_receive(ServiceM, msg, MAX_MSG_Size);

        int msgtype = messageType(msg);

        switch (msgtype)
        {
        case 20: // DISCOVERY
            char *NOTMSG = NotificationGenerate(thisNode->serviceName, thisNode->AttributeList, thisNode->attr_num);
            SendWaveMsg(AppM, NOTMSG, AD_Send_Interval, AD_Post_Num);
            break;
        default:
            break;
        }
    }
    return;
}

/* Thread function that keeps sending HEARTBEAT msg. */
void *HBSenderThread()
{
    // in App
    char *HBmsg = HeartBeatGenerate(thisNode->serviceName);
    while (join_threads == 0)
    {
        usleep(10000);
        
        SendMsg(AppM, HBmsg);
    }
}

// char *getIP()
// {
//     char hostname[1024];
//     struct hostent *host_entry;
//     char *ip;

//     // Get the hostname
//     if (gethostname(hostname, sizeof(hostname)) == -1)
//     {
//         perror("gethostname");
//         return 1;
//     }

//     // Get hostent structure for the hostname
//     if ((host_entry = gethostbyname(hostname)) == NULL)
//     {
//         perror("gethostbyname");
//         return 1;
//     }

//     // Convert the IP address to a string
//     ip = inet_ntoa(*(struct in_addr *)host_entry->h_addr_list[0]);

//     printf("Hostname: %s\n", hostname);
//     printf("IP Address: %s\n", ip);

//     return ip;
// }

int zcs_init(int type)
{
    // MulticastConfig = "ip#sport#rport"
    AppM = multicast_init(LanIp, APPRPORT, APPSPORT);
    ServiceM = multicast_init(LanIp, SERVICERPORT, SERVICESPORT);
    if (AppM == NULL || ServiceM == NULL)
    {
        return -1;
    }
    if (type != ZCS_APP_TYPE && type != ZCS_SERVICE_TYPE)
    {
        return -1;
    }


    Nodetype = type;
    if (type == ZCS_APP_TYPE)
    {
        LocalR = (LocalRegistry *)malloc(MAX_SERVICES * sizeof(LocalRegistry));
        AdListenDict = (AdCallbackListenDict *)malloc(MAX_SERVICES * sizeof(AdCallbackListenDict));
        if (LocalR == NULL || AdListenDict == NULL)
        {
            return -1;
        }
        SendWaveMsg(ServiceM,"DISCOVERY#APP",AD_Send_Interval,AD_Post_Num);
        pthread_create(&ListenerThread, NULL, AppListenThread, NULL);
        
        isInit = 1;
    }
    else if (type == ZCS_SERVICE_TYPE)
    {
        isInit = 1;
    }
    return 0;
};

LocalRegistry *initializeNode(const char *name, const zcs_attribute_t attr[], int num)
{
    // Allocate memory for Node
    LocalRegistry *newNode = (LocalRegistry *)malloc(sizeof(LocalRegistry));
    if (newNode == NULL)
    {
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
    for (int i = 0; i < num; i++)
    {
        newNode->AttributeList[i].attr_name = strdup(attr[i].attr_name);
        newNode->AttributeList[i].value = strdup(attr[i].value);
    }

    return newNode;
}

int zcs_start(char *name, zcs_attribute_t attr[], int num)
{
    if (isInit == 0)
    {
        return -1;
    }
    
    if (Nodetype == ZCS_SERVICE_TYPE)
    { // Service
        thisNode = initializeNode(name, attr, num);
        char *NOTMSG = NotificationGenerate(thisNode->serviceName, thisNode->AttributeList, thisNode->attr_num);
        SendWaveMsg(AppM, NOTMSG, AD_Send_Interval, AD_Post_Num);
        pthread_create(&ListenerThread, NULL, ServiceListenThread, NULL);
        pthread_create(&HeartBeatGenerateThread, NULL, HBSenderThread, NULL);
        
    }
    // Do nothing is it's APP_TYPE

    return 0;
}

int zcs_post_ad(char *ad_name, char *ad_value)
{
    if (thisNode == NULL)
    {
        //error, this is app/ not start
        return -2;
    }
    char *ADMessage = AdvertisementGenerate(ad_name, ad_value);
    int c = SendWaveMsg(AppM,ADMessage,AD_Send_Interval,AD_Post_Num);

    return c;
};

int zcs_query(char *attr_name, char *attr_value, char *node_names[], int namelen)
{
    int count = 0;
    for (int i = 0; i < MAX_SERVICES; i++)
    {
        if (LocalR[i].serviceName != NULL)
        {
            for (int j = 0; j < MAX_SERVICE_ATTRIBUTE; j++)
            {
                if (strcmp(attr_name, LocalR[i].AttributeList[j].attr_name) == 0 &&
                    strcmp(attr_value, LocalR[i].AttributeList[j].value) == 0)
                {
                    strcpy(node_names[count++], LocalR[i].serviceName);
                }
            }
        }
    }
    return count;
};

int zcs_get_attribs(char *name, zcs_attribute_t attr[], int *num)
{
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SERVICES; i++)
    {
        if (LocalR[i].serviceName != NULL && strcmp(LocalR[i].serviceName, name) == 0)
        {
            for (int count = 0; count < *num; count++)
            {
                strcpy(attr[count].attr_name, LocalR[i].AttributeList[count].attr_name);
                strcpy(attr[count].attr_name, LocalR[i].AttributeList[count].value);
                pthread_mutex_unlock(&mutex);
                return 0;
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1;
};

int zcs_listen_ad(char *name, zcs_cb_f cback){
    for (int i = 0; i < MAX_MSG_Size; i++)
    {
        if (AdListenDict[i].SName != NULL && strcmp(AdListenDict[i].SName, name) == 0)
        {
            AdListenDict[i].callback = cback;
        }
        else if (AdListenDict[i].SName == NULL)
        {
            AdListenDict[i].SName = name;
            AdListenDict->callback = cback;
        }
    }
};

int zcs_shutdown()
{
    if (isInit == 0 || thisNode == NULL) return -1;
    int errCode;
    join_threads = 1;
    if (Nodetype == ZCS_SERVICE_TYPE)
    {
        errCode = pthread_join(HeartBeatGenerateThread,NULL);
        if (errCode != 0) return -1;
    }
    errCode = pthread_join(ListenerThread,NULL);
    return errCode;
};

void zcs_log()
{
    printf("====== Log ======\n");
    for (int i = 0; i < MAX_SERVICES; i++)
    {
        if (LocalR[i].serviceName != NULL)
        {
            printf("Name: %s, State: %s, Attributes: ", LocalR[i].serviceName, LocalR[i].isAlive);
            for (int j = 0; j < LocalR[i].attr_num; j++)
            { // print all attributes of current node
                if (LocalR[i].AttributeList[j].attr_name == NULL)
                {
                    printf("\n");
                    break;
                }
                printf("(%s,%s), ", LocalR[i].AttributeList[j].attr_name, LocalR[i].AttributeList[j].value);
            }
        }
    }
};