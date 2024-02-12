#ifndef __ZCS_H__
#define __ZCS_H__

#define ZCS_APP_TYPE                1
#define ZCS_SERVICE_TYPE            2

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
    zcs_cb_f callbackFunc;
}addCallbackListenDict;

#define MAX_SERVICES 100
#define MAX_SERVICE_ATTRIBUTE 10

#define APPSPORT 2024 
#define APPRPORT 2025 

#define SERVICESPORT 2019 
#define SERVICERPORT 2020 

#define MAX_MSG_Size 2048
#define MAX_NODE_NAME_SIZE 64

#define TIMEOUT 1




// Structure to hold service ID and its status
typedef struct {
    char serviceName[MAX_NODE_NAME_SIZE];
    int attr_num;
    int isAliveTimeCount; // count for time receive counter
    int isAlive; // 0 for false, 1 for true
    zcs_attribute_t AttributeList[MAX_SERVICE_ATTRIBUTE];
}LocalRegistry;


typedef void (*zcs_cb_f)(char *, char *);

int zcs_init(int type , char *MulticastConfig);
int zcs_start(char *name, zcs_attribute_t attr[], int num);
int zcs_post_ad(char *ad_name, char *ad_value);
int zcs_query(char *attr_name, char *attr_value, char *node_names[], int namelen);
int zcs_get_attribs(char *name, zcs_attribute_t attr[], int *num);
int zcs_listen_ad(char *name, zcs_cb_f cback);
int zcs_shutdown();
void zcs_log();

#endif

