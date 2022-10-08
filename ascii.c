#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <ctype.h>


void get_string(char buf[], int size){
    char c;
    fgets(buf, size, stdin);

    if(buf[strlen(buf) -1] == '\n'){
        buf[strlen(buf)-1] = 0;
    }
    else
        while((c = getchar()) != '\n' && c != EOF);
}

int main(void){

    char buf[255];
    get_string(buf, 255);
    int lengde = strlen(buf);
    int ugyldigAsci = 0;
    for(int i = 0; i < lengde; i++){
        if(!isascii(buf[i]) || isspace(buf[i])){
            ugyldigAsci++;
        }
    }
    if(ugyldigAsci > 0){
        printf("ugyldig asci verdi\n");
    }

    return 0;
}