#include "zcs.h"
#include <stdio.h>
#include <pthread.h>
#include "multicast.h"


int isInit = 0; // 0 for false, 1 for true
char *LanIp ; 
mcast_t *AppM;
mcast_t *ServiceM;
LocalRegistry *LocalR;
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

void HeartBeatGenerate() {
    while(1) {
        //receive
    }
    return ;
    
}

void NotificationGenerate() {
    while(1) {
        //receive
    }
    return ;
    
}

void SendMsg(mcast_t Destination, char msg[]) {
    
    return ;
    
}

int zcs_init(int type , char *MulticastConfig){
    //MulticastConfig = "ip#sport#rport"

    AppM = multicast_init(LanIp, APPRPORT, APPSPORT);
    ServiceM = multicast_init(LanIp, SERVICERPORT, SERVICESPORT);
    if (type == ZCS_APP_TYPE)
    {
        LocalR = malloc(MAX_SERVICES*sizeof(LocalRegistry));
        isInit = 1;

        


    }
    else if (type == ZCS_SERVICE_TYPE)
    {
        isInit = 1;
    }
    else{
        
    }
    return -1;
    
};

int zcs_post_ad(char *ad_name, char *ad_value){

    //post ad name and val 
};

int zcs_query(char *attr_name, char *attr_value, char *node_names[]){};

int zcs_get_attribs(char *name, zcs_attribute_t attr[], int *num){};

int zcs_listen_ad(char *name, zcs_cb_f cback){};

int zcs_shutdown(){};

void zcs_log(){};