#include "kernel/types.h"
#include "user.h"

int main(int argc,char* argv[]){
    if(argc != 2){
        fprintf(2,"Sleep needs one argument!\n");
        exit(-1);
    }
    int ticks = atoi(argv[1]);
    sleep(ticks);
    printf("(nothing happens for a little while)\n");
    exit(0);
}