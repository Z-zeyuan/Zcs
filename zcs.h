#ifndef __ZCS_H__
#define __ZCS_H__

#define ZCS_APP_TYPE                1
#define ZCS_SERVICE_TYPE            2

#define MAX_SERVICES 100            // Max number of nodes in local registry
#define MAX_SERVICE_ATTRIBUTE 10    // Max number of attributes of a node

#define APPSPORT 2024 
#define APPRPORT 2025 

#define SERVICESPORT 2019 
#define SERVICERPORT 2020 

#define MAX_MSG_Size 2048           // Max size of a msg
#define MAX_NODE_NAME_SIZE 64       // Max size of a node's name

#define TIMEOUT 1                   // Time interval for checking nodes' state

typedef void (*zcs_cb_f)(char *, char *);

typedef struct {
    char *attr_name;
    char *value;
} zcs_attribute_t;

typedef struct {
    char *name;
    int count;
} dict;

typedef struct {
    char *SName;
    zcs_cb_f callback;
} AdCallbackListenDict;

// Structure to hold service ID and its status
typedef struct {
    char serviceName[MAX_NODE_NAME_SIZE];
    int attr_num;                   // Number of attributes
    int isAliveTimeCount;           // count for time receive counter
    int isAlive;                    // 0 for false, 1 for true
    zcs_attribute_t AttributeList[MAX_SERVICE_ATTRIBUTE];   // List of attributes
}LocalRegistry;


int zcs_init(int type , char *MulticastConfig);
int zcs_start(char *name, zcs_attribute_t attr[], int num);
int zcs_post_ad(char *ad_name, char *ad_value);
int zcs_query(char *attr_name, char *attr_value, char *node_names[], int namelen);
int zcs_get_attribs(char *name, zcs_attribute_t attr[], int *num);
int zcs_listen_ad(char *name, zcs_cb_f cback);
int zcs_shutdown();
void zcs_log();

#endif

