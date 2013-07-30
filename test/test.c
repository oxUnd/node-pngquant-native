#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *args[]) {
    char * str_buffer = "-d -r -g";

    char *argv[32] = {""};


    
    char token[64];
    memset(token, '\0', 64);

    int i = 0, j = 0, k = 0, len = 0;
    while(str_buffer[i] != '\0') {
        if (str_buffer[i] == ' ') {
            j++;
            k = 0;
            len = strlen(token);
            argv[j] = (char*) malloc(len + 1);
            memset(argv[j], '\0', len + 1);
            memcpy(argv[j], token, len + 1);
        } else {
            token[k++] = str_buffer[i];
        }
        i++;
    }
    if ((len = strlen(token)) > 0) {
        j++;
        argv[j] = (char*) malloc(len + 1);
        memset(argv[j], '\0', len + 1);
        memcpy(argv[j], token, len + 1);
    }

}