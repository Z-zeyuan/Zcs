#include <stdio.h>
#include <unistd.h>
#include "zcs.h"

int main(){
    // Service node test 
    sleep(10);
    zcs_attribute_t attribs[] = {
	    { .attr_name = "type", .value = "speaker"},
	    { .attr_name = "location", .value = "home"},
	    { .attr_name = "make", .value = "yamaha"} };

    int a = zcs_init(ZCS_SERVICE_TYPE);
    int b = zcs_start("speaker-2", attribs, sizeof(attribs)/sizeof(zcs_attribute_t));
    //printf("st: a = %d, b = %d\n",a,b);
    sleep(60);
    return 0;
}