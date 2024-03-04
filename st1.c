#include <stdio.h>
#include <unistd.h>
#include "zcs.h"

int main(){
    // Service node test 
    sleep(2);
    zcs_attribute_t attribs[] = {
	    { .attr_name = "type", .value = "dishwasher"},
	    { .attr_name = "location", .value = "home"},
	    { .attr_name = "make", .value = "random"} };

    int a = zcs_init(ZCS_SERVICE_TYPE);
    int b = zcs_start("dishwasher-1", attribs, sizeof(attribs)/sizeof(zcs_attribute_t));
    //printf("st: a = %d, b = %d\n",a,b);
    sleep(60);
    return 0;
}