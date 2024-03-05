#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "zcs.h"
#include "multicast.h"

// ========== Definitions ====================================================================================================

#define MAX_NODES 100               // Max number of nodes in local registry
#define MAX_SERVICE_ATTRIBUTE 10    // Max number of attributes of a node
#define MAX_CALLBACK_NUM 10         // Max CallBack function that a node can register
#define MAX_MSG_LENGTH 1024        
#define MAX_NODE_NAME_LENGTH 64 

#define APP_SEND_PORT   4096
#define SERVICE_SEND_PORT 5024


#define RELAY_TOAPP_PORT   6000
#define RELAY_TOSERVICE_PORT 7000

#define DEFAULT_PORT 4004

#define TIMEOUT 1

// ========== Struct definitions ==========================================================================================

typedef struct {
    char *name;
    int count;
} HBCounter;

typedef struct{
    int count;
    HBCounter counters[MAX_NODES];
}HBDict;

typedef struct {
    char *SName;
    zcs_cb_f callback;
} CBDictEntry;   // Callback dict entry

typedef struct {
    int count;
    CBDictEntry cbs[MAX_CALLBACK_NUM];
}CBDict;

// Structure to hold service ID and its status
typedef struct {
    char *name;                 
    int isAlive;                    // Flag indicating whether it's alive
    zcs_attribute_t *attributes;    // List of zcs_attribute_t structs
    int attrnum;                    // Number of zcs_attribute_t structs in the list
} Node;

typedef struct {
    Node nodes[MAX_NODES];
    int num_nodes;
} NodeList;

// ========== Global Variables ====================================================================================================

char *ip1A = "224.1.1.1";
char *ip1S = "224.1.1.2";
char *Relay_ip = "224.1.10.10";

char *discovery = "D";

int isInit = 0;
int Nodetype;

mcast_t *App_Service_Send;
mcast_t *App_Service_Receive;
mcast_t *Service_App_Send;
mcast_t *Service_App_Receive;

mcast_t *Send_to_Relay;


NodeList LR;    // Local Registry
CBDict cbd;     // CallBack function table
HBDict hbd;     // Heartbeat Counter table
Node thisnode; 

int join_threads = 0;
pthread_mutex_t mutex;
pthread_t ListenerThread;           
pthread_t HeartBeatGenerateThread;


// ========== Struct helpers ==============================================================================================================


/* Create a new Node*/
void initializeNode(Node *node, const char *name, zcs_attribute_t attr[], int num) {
    node->name = strdup(name);
    node->isAlive = 0;
    node->attrnum = (num < MAX_NODES) ? num : MAX_NODES;
    node->attributes = malloc(num * sizeof(zcs_attribute_t));
    if (node->attributes == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < node->attrnum; i++){
        node->attributes[i] = attr[i];
    }
}

void freeNode(Node *node) {
    free(node->name);
    for (int i = 0; i < node->attrnum; i++) {
        free(node->attributes[i].attr_name);
        free(node->attributes[i].value);
    }
    free(node->attributes);
}

void initializeNodeList(NodeList *list) {
    list->num_nodes = 0;
}

int addNode(NodeList *list, Node node) {
    pthread_mutex_lock(&mutex);
    // Check if a node with the same name already exists
    for (int i = 0; i < list->num_nodes; i++) {
        if (strcmp(list->nodes[i].name, node.name) == 0) {
            pthread_mutex_unlock(&mutex);
            return -1; // Node with the same name already exists, do nothing
        }
    }
    if (list->num_nodes >= MAX_NODES) {
        fprintf(stderr, "Maximum number of nodes reached.\n");
        pthread_mutex_unlock(&mutex);
        return -2;
    }  
    // Add the node to the list
    list->nodes[list->num_nodes++] = node;

    pthread_mutex_unlock(&mutex);
    return 0;
}

void initializeCBD(CBDict *dict){
    dict->count = 0;
}

int findCallbackIndex(CBDict *dict, char *name) {
    for (int i = 0; i < dict->count; i++) {
        if (strcmp(dict->cbs[i].SName, name) == 0) {
            return i; // Name found, return its index
        }
    }
    return -1; // Name not found
}

void addCallBack(CBDict *dict, char *name, zcs_cb_f cb) {
    if (dict->count >= MAX_CALLBACK_NUM) {
        printf("addCallBack: Full\n");
        return;
    }

    int index = findCallbackIndex(dict, name);
    if (index != -1) {
        printf("addCallBack: Name already exists\n");
        return;
    }

    index = dict->count;
    dict->cbs[index].SName = name;
    dict->cbs[index].callback = cb;
    dict->count++;
}

void printNode(Node *node){
    if (node != NULL) {
        printf("Node:\n");
        printf("Name: %s\n", node->name);
        printf("IsAlive: %d\n", node->isAlive);
        printf("Attribute Count: %d\n", node->attrnum);
        for (int i = 0; i < node->attrnum; i++) {
            printf("Attribute %d: %s = %s\n", i+1, node->attributes[i].attr_name, node->attributes[i].value);
        }
    } else {
        printf("Failed to print node.\n");
    }
}

void initializeHBDict(HBDict *hbd){
    hbd->count = 0;
}

void increaseHBCount(HBDict *hbd, char *name) {
    // Check if the HBCounter with the given name exists
    for (int i = 0; i < hbd->count; i++) {
        if (strcmp(hbd->counters[i].name, name) == 0) {
            // If found, increase its counter by 1
            hbd->counters[i].count++;
            return;
        }
    }
    // If not found, create a new HBCounter
    if (hbd->count < MAX_NODES) {
        hbd->counters[hbd->count].name = strdup(name); // Allocate memory for the name
        hbd->counters[hbd->count].count = 1;
        hbd->count++;
    } else {
        printf("Error: Maximum number of HBCounters reached.\n");
    }
}

void updateLR(NodeList *lr, HBDict *hbd) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < hbd->count; i++) {
        HBCounter *counter = &hbd->counters[i];

        // Find corresponding node in lr by name
        for (int j = 0; j < lr->num_nodes; j++) {
            Node *node = &lr->nodes[j];
            if (strcmp(node->name, counter->name) == 0) {
                // Update isAlive flag based on counter's count
                if (counter->count > 3) {
                    node->isAlive = 1;
                } else {
                    node->isAlive = 0;
                }
                counter->count = 0;
                break;  // No need to continue searching once found
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}


// ========== Message Encode & Decode ====================================================================================================


int getMsgType(char *str) {
    char firstChar = str[0];
    switch(firstChar) {
        case 'D':
            return 1;
        case 'N':
            return 2;
        case 'H':
            return 3;
        case 'A':
            return 4;
        default:
            return -1;
    }
}

char *GenerateNotificationMsg(Node node) {
    // Calculate the length of the output string
    int length = snprintf(NULL, 0, "N#%s#%d#", node.name, node.attrnum);

    // Calculate the length of attributes part of the string
    for (int i = 0; i < node.attrnum; i++) {
        length += snprintf(NULL, 0, "%s#%s#", node.attributes[i].attr_name, node.attributes[i].value);
    }

    // Allocate memory for the output string
    char *msg = (char *)malloc(length + 1);
    if (msg == NULL) {
        // Memory allocation failed
        return NULL;
    }

    // Generate the output string
    int offset = sprintf(msg, "N#%s#%d#", node.name, node.attrnum);
    for (int i = 0; i < node.attrnum; i++) {
        offset += sprintf(msg + offset, "%s#%s#", node.attributes[i].attr_name, node.attributes[i].value);
    }

    return msg;
}

Node *DecodeNotificationMsg(char *msg) {
    Node *node = malloc(sizeof(Node));
    if (node == NULL) {
        return NULL; // Memory allocation failed
    }

    // Initialize attributes pointer to NULL to handle free if needed
    node->attributes = NULL;

    // Parsing the message, get the msg type
    char *token = strtok(msg, "#");
    if (token == NULL || strcmp(token, "N") != 0) {
        free(node);
        return NULL; // Invalid message format
    }

    // Extract node name
    token = strtok(NULL, "#");
    if (token == NULL || strlen(token) > MAX_NODE_NAME_LENGTH) {
        free(node);
        return NULL; // Invalid message format
    }
    node->name = strdup(token); // Duplicate the string since strtok modifies the input

    // Extract attribute count
    token = strtok(NULL, "#");
    if (token == NULL) {
        free(node->name);
        free(node);
        return NULL; // Invalid message format
    }
    node->attrnum = atoi(token);
    if (node->attrnum < 0) {
        free(node->name);
        free(node);
        return NULL; // Negative attribute count
    }

    // Allocate memory for attributes
    node->attributes = malloc(node->attrnum * sizeof(zcs_attribute_t));
    if (node->attributes == NULL) {
        free(node->name);
        free(node);
        return NULL; // Memory allocation failed
    }

    // Extract attributes
    for (int i = 0; i < node->attrnum; i++) {
        token = strtok(NULL, "#"); // Attribute name
        if (token == NULL) {
            free(node->name);
            free(node->attributes);
            free(node);
            return NULL; // Invalid message format
        }
        node->attributes[i].attr_name = strdup(token);

        token = strtok(NULL, "#"); // Attribute value
        if (token == NULL) {
            free(node->name);
            for (int j = 0; j < i; j++) {
                free(node->attributes[j].attr_name);
            }
            free(node->attributes);
            free(node);
            return NULL; // Invalid message format
        }
        node->attributes[i].value = strdup(token);
    }

    //  isAlive is 0 by default
    node->isAlive = 0;

    return node;
}

char *DecodeHeartBeatMSG(char *msg){
    char *token = strtok(msg, "#");
    // If token is not NULL, return the next token (which is the name)
    if (token != NULL) {
        token = strtok(NULL, "#"); // Get the next token after '#'
        return token;
    }
    return NULL; // Return NULL if no token is found
}

char *GenerateAdvertisementMSG(char *node_name, char *ad_name, char *ad_value) {
    // Calculate the length of the resulting string
    size_t len = strlen("A#") + strlen(node_name) + strlen(ad_name) + strlen(ad_value) + 3; // +3 for "#" separators and null terminator

    // Allocate memory for the resulting string
    char *msg = (char *)malloc(len * sizeof(char));
    if (msg == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL; // Return NULL if memory allocation fails
    }

    // Format the output string
    snprintf(msg, len, "A#%s#%s#%s", node_name, ad_name, ad_value);

    return msg;
}

void ProcessAdvertisementMSG(char *msg){
    char *token = strtok(msg, "#");
    if (token == NULL || strcmp(token, "A") != 0) {
        printf("Invalid message format.\n");
        return;
    }

    token = strtok(NULL, "#");
    if (token == NULL) {
        printf("Node name not found.\n");
        return;
    }
    char nodeName[MAX_NODE_NAME_LENGTH];
    strncpy(nodeName, token, MAX_NODE_NAME_LENGTH - 1);
    nodeName[MAX_NODE_NAME_LENGTH - 1] = '\0';

    token = strtok(NULL, "#");
    if (token == NULL) {
        printf("AdName not found.\n");
        return;
    }
    char adName[100];
    strncpy(adName, token, sizeof(adName) - 1);
    adName[sizeof(adName) - 1] = '\0';

    token = strtok(NULL, "#");
    if (token == NULL) {
        printf("AdValue not found.\n");
        return;
    }
    char adValue[100];
    strncpy(adValue, token, sizeof(adValue) - 1);
    adValue[sizeof(adValue) - 1] = '\0';

    int index = findCallbackIndex(&cbd, nodeName);

    if (index == -1) {
        printf("CBDictEntry for node '%s' not found.\n", nodeName);
        return;
    }

    cbd.cbs[index].callback(adName, adValue);

}

// ========== Multicast functions ==============================================================================================================

/* Send msg to channel 'wave' times. */
void send_msg(mcast_t *channel, void *msg, int msglen, int wave){
    for (int i = 0; i < wave; i++){
        int err = multicast_send(channel,msg,msglen);
        err = multicast_send(Send_to_Relay,msg,msglen);
        //printf("Message Send: %d\n",err);
        usleep(10000);
    }
}


// ========== Thread functions ==============================================================================================================

void *AppListener(){
    // === Initialization ===
    initializeHBDict(&hbd);
    time_t start_time;
    int reset_time = 1;
    multicast_setup_recv(App_Service_Receive);
    char msg[MAX_MSG_LENGTH];
    while(join_threads == 0){
        // === Try to receive message ===
        if (reset_time == 1){
            reset_time = 0;
            start_time = time(NULL);
        }
        //printf("AppListener is alive...\n");
        memset(msg,'\0',sizeof(msg));
        while(multicast_check_receive(App_Service_Receive) == 0){
            //printf("App is waiting for message...\n");
            if (difftime(time(NULL),start_time) > TIMEOUT){
                updateLR(&LR,&hbd);
                //zcs_log();
                reset_time = 1;
            }
        }
        multicast_receive(App_Service_Receive,msg,MAX_MSG_LENGTH);
        //printf("Services's new msg: %s\n",msg);
        // === Process message ===
        int msgtype = getMsgType(&msg);
        switch (msgtype)
        {
        case 2:     // Notification
            // Get the node and add it to LR
            int errcode = -1;
            Node *node = DecodeNotificationMsg(&msg);
            if (node != NULL) errcode = addNode(&LR,*node);
            if (errcode == 0) {
                increaseHBCount(&hbd,node->name);
                //zcs_log();
            }
            break;
        case 3:     // HeartBeat
            // Get service name and increase HBCounter in HBDict
            increaseHBCount(&hbd,DecodeHeartBeatMSG(&msg));
            break;
        case 4: // Advertisement
            ProcessAdvertisementMSG(msg); 
            break;
        default:
            break;
        }
        if (difftime(time(NULL),start_time) > TIMEOUT){
                updateLR(&LR,&hbd);
                //zcs_log();
                reset_time = 1;
        }
    }
}

void *ServiceListener(){
    multicast_setup_recv(Service_App_Receive);
    char msg[MAX_MSG_LENGTH];
    while(join_threads == 0){
        //printf("ServiceListener is alive...\n");
        memset(msg,'\0',sizeof(msg));
        while(multicast_check_receive(Service_App_Receive) == 0){
            //printf("Service is waiting for message...\n");
        }
        multicast_receive(Service_App_Receive,msg,MAX_MSG_LENGTH);
        printf("App's new msg: %s\n",msg);
        int msgtype = getMsgType(&msg);
        if(msgtype == 1){   // Discovery Message
            char *notMsg = GenerateNotificationMsg(thisnode);
            send_msg(Service_App_Send,notMsg,strlen(notMsg),1);
        }
    }
}

void *HeartBeatGenerator(){
    char HBmsg[100];
    sprintf(HBmsg,"H#%s",thisnode.name);
    while(join_threads == 0){
        //printf("HeartBeatGenerator is alive...\n");
        send_msg(Service_App_Send,&HBmsg,strlen(HBmsg),1);
        usleep(10000);
    }
}

// ========== Main features ==========

/*  Initializes the ZCS library. 
    The input 'type' indicates whether an app or service is being initialized. 
    Return 0 if the initialization was a success. 
    Return -1 otherwise.
*/
int zcs_init(int type){
    if (type != ZCS_APP_TYPE && type != ZCS_SERVICE_TYPE) return -1;    // Incorrect input value
    Nodetype = type;

    if(Nodetype == ZCS_APP_TYPE){
        // Initialize Local Registry Table for App
        initializeNodeList(&LR);
        // Initialize CallBack Table
        initializeCBD(&cbd);
        // Initialize multicast channels
        App_Service_Send = multicast_init(ip1A,APP_SEND_PORT,DEFAULT_PORT);
        App_Service_Receive = multicast_init(ip1S,DEFAULT_PORT,SERVICE_SEND_PORT);
        Send_to_Relay = multicast_init(Relay_ip,RELAY_TOSERVICE_PORT,DEFAULT_PORT);
        // Create Listener thread
        pthread_create(&ListenerThread,NULL,AppListener,NULL);
        // Send Discovery Msg
        send_msg(App_Service_Send,discovery,strlen(discovery),1);
        isInit = 1;
    }else{
        // Initialize multicast channels for Service
        Service_App_Send = multicast_init(ip1S,SERVICE_SEND_PORT,DEFAULT_PORT);
        Service_App_Receive = multicast_init(ip1A,DEFAULT_PORT,APP_SEND_PORT);
        Send_to_Relay = multicast_init(Relay_ip,RELAY_TOAPP_PORT,DEFAULT_PORT);

        isInit = 1;
    }

    return 0;
}

/*  Puts a node online. (Supposed to be called by Service type Node)
    'name' must be an ASCII string without spaces that is NULL terminated, and its max length is 64.
    The attributes are specified as key-value pairs and would remain unchanged until the node shuts down.
    'num' specifies the number of attributes passed into the node.
    Retrun 0 if the node start was a success.
    Return -1 otherwise.
*/
int zcs_start(char *name, zcs_attribute_t attr[], int num){
    if (isInit == 0) return -1;

    if (Nodetype == ZCS_SERVICE_TYPE){
        // Create a node for myself
        initializeNode(&thisnode,name,attr,num);
        //printNode(&thisnode);
        // Create a notification message based on myself
        char *msg = GenerateNotificationMsg(thisnode);
        //printf("Notification msg: %s\n",msg);
        // Initialize threads and send notification message
        pthread_create(&ListenerThread,NULL,ServiceListener,NULL);
        send_msg(Service_App_Send,msg,strlen(msg),1);
        pthread_create(&HeartBeatGenerateThread,NULL,HeartBeatGenerator,NULL);
    }
    return 0;
}
/*  This function broadcast an advertisement message with the given name and value to all app nodes.
    Return -1 if ZCS Library is not initialized or the function is not called by a service node.
    Return number of times the advertisement was posted on the network.
*/
int zcs_post_ad(char *ad_name, char *ad_value){
    if(isInit == 0 || Nodetype != ZCS_SERVICE_TYPE) return -1;
    char *msg = GenerateAdvertisementMSG(thisnode.name,ad_name,ad_value);
    int count = 5;
    send_msg(Service_App_Send,msg,strlen(msg),count);
    return count;
}

/*  This function is used to scan for nodes with a given value for a given attribute.
    The names of the nodes found are stored in the node_names, with a max size of "num".
    Return x where x is number of nodes found.
*/
int zcs_query(char *attr_name, char *attr_value, char *node_names[], int num){
    if (isInit == 0 || &LR == NULL) return 0;
    //printf(attr_name);
    //printf(attr_value);
    int count = 0;
    for (int i = 0; i < LR.num_nodes; i++){
        if (count >= num) break;
        for (int j = 0; j < LR.nodes[i].attrnum; j++){
            if (strcmp(LR.nodes[i].attributes[j].attr_name,attr_name) == 0 && 
                strcmp(LR.nodes[i].attributes[j].value,attr_value) == 0){
                if (count < num) {
                    // Copy the node name to the node_names array
                    node_names[count] = (char *)malloc(MAX_NODE_NAME_LENGTH);
                    memset(node_names[count],'\0',sizeof(node_names[count]));

                    strcpy(node_names[count], LR.nodes[i].name);
                    //node_names[count][MAX_NODE_NAME_LENGTH - 1] = '\0'; // Null-terminate the string
                    count++;
                    break;
                }  
            }
        }
    }
    printf("%s\n",node_names[0]);
    return count;
}

/*  This function is used to get the full list of attributes of a node.
    The first argument is the name of the node.
    The second argument is an attribute array that is already allocated.
    The third argument is set to the number of slots allocated in the attribute array. The function sets it to the number of actual attributes read from the node.
    Return 0 if there's no error.
    Return -1 if error occurs.
*/
int zcs_get_attribs(char *name, zcs_attribute_t attr[], int *num){
    int node_index = -1;
    //printf("%s\n",name);
    // Find the index of the node with the given name
    for (int i = 0; i < LR.num_nodes; i++) {
        if (strcmp(LR.nodes[i].name, name) == 0) {
            node_index = i;
            //printf("Node Found\n");
            break;
        }
    }
    
    if (node_index == -1) {
        printf("Node '%s' not found.\n", name);
        return -1; // Return -1 if the node is not found
    }

    // Copy attributes to the provided attribute array
    int count = 0;
    for (int i = 0; i < LR.nodes[node_index].attrnum; i++) {
        // Allocate memory for the key and value strings
        attr[count].attr_name = malloc(strlen(LR.nodes[node_index].attributes[i].attr_name) + 1);
        //memset(attr[count].attr_name,'\0',sizeof(attr[count].attr_name));
        attr[count].value = malloc(strlen(LR.nodes[node_index].attributes[i].value) + 1);
        //memset(attr[count].value,'\0',sizeof(attr[count].value));
        if (attr[count].attr_name == NULL || attr[count].value == NULL) {
            // Memory allocation failed, free previously allocated memory
            for (int j = 0; j < count; j++) {
                free(attr[j].attr_name);
                free(attr[j].value);
            }
            return -1; // Return -1 if memory allocation fails
        }
        // Copy key and value strings
        strcpy(attr[count].attr_name, LR.nodes[node_index].attributes[i].attr_name);
        strcpy(attr[count].value, LR.nodes[node_index].attributes[i].value);
        count++;
    }

    // Update the number of actual attributes read
    *num = count;
    //printf("%s\n",attr[0].attr_name);
    return 0;
}

/*  This function takes two arguments. 
    The first is a name of the target node and the second is the callback that will be triggered when the target posts an advertisement. 
    The callback has two arguments: name of the advertisement and the value of the advertisement.
*/
int zcs_listen_ad(char *name, zcs_cb_f cback){
    addCallBack(&cbd,name,cback);
}

/*  This function is called to terminate the activities of the ZCS by a program before it terminates.
    Return 0 if success.
    Return -1 if error occurs.
*/
int zcs_shutdown(){
    if (isInit == 0) return -1;
    int errCode;
    join_threads = 1;
    if (Nodetype == ZCS_SERVICE_TYPE)
    {
        errCode = pthread_join(HeartBeatGenerateThread,NULL);
        if (errCode != 0) return errCode;
    }
    errCode = pthread_join(ListenerThread,NULL);
    return errCode;
}

void zcs_log(){
    if (&LR == NULL) return;
    pthread_mutex_lock(&mutex);
    printf("NodeList Information:\n");
    printf("Number of nodes: %d\n", LR.num_nodes);
    
    for (int i = 0; i < LR.num_nodes; i++) {
        printf("Node %d:\t", i + 1);
        printf("Name: %s\t", LR.nodes[i].name);
        printf("isAlive: %d\t", LR.nodes[i].isAlive);
        printf("attrnum: %d\n",LR.nodes[i].attrnum);
        printf("Attributes:\n");
        for (int j = 0; j < LR.nodes[i].attrnum; j++) {
            printf("(%s: %s);", LR.nodes[i].attributes[j].attr_name, LR.nodes[i].attributes[j].value);
        }
        printf("\n");
    }
    pthread_mutex_unlock(&mutex);
}