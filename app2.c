#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "zcs.h"

void hello(char *s, char *r) {
    printf("Ad received: %s, with value: %s\n", s, r);
    //zcs_log();
}

int main() {
    int rv;
    sleep(20);
    rv = zcs_init(ZCS_APP_TYPE);
    char *names[10];
    printf("yutvfgubyh76ftrcdxgfvyubh867frctdgfbuy6g7t\n");
    
    rv = zcs_query("type", "speaker", names, 10);
    sleep(5);
    //printf("%d\n",rv);
    if (rv > 0) {
        printf("qurey good\n");
        zcs_attribute_t attrs[5];
	    int anum = 5;
        rv = zcs_get_attribs(names[0], attrs, &anum);
        if ((strcmp(attrs[1].attr_name, "location") == 0) &&
            (strcmp(attrs[1].value, "home") == 0)) {
                printf("11111\n");
                rv = zcs_listen_ad(names[0], hello);
        }
    }
    zcs_log();
    sleep(100);
    zcs_log();
}


