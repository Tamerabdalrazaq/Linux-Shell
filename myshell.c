#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int prepare(){
    printf("prepare");
    return 0;
}

int finalize(void){
    printf("finalize");
    return 0;
}

int process_arglist(void){
    printf("process_arglist");
    return 0;
}
