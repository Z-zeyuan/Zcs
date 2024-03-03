#include <stdio.h>
#include <unistd.h>
#include "zcs.h"

int main(){
    sleep(5);
    int a = zcs_init(ZCS_APP_TYPE);
    sleep(80);
    return a;
}