#include <stdio.h>
#include <unistd.h>
#include "zcs.h"

int main(){
    // Service node test 
    
    zcs_attribute_t attribs[] = {
	    { .attr_name = "type", .value = "speaker"},
	    { .attr_name = "location", .value = "kitchen"},
	    { .attr_name = "make", .value = "yamaha"} };

    int a = zcs_init(ZCS_SERVICE_TYPE);
    int b = zcs_start("speaker-X", attribs, sizeof(attribs)/sizeof(zcs_attribute_t));
    //printf("st: a = %d, b = %d\n",a,b);
    sleep(15);
    return 0;
}