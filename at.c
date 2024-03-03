#include <stdio.h>
#include <unistd.h>
#include "zcs.h"

int main(){
    
    int a = zcs_init(ZCS_APP_TYPE);
    sleep(15);
    return a;
}