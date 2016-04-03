#ifndef CHECKSUM_UTILS_H
#define CHECKSUM_UTILS_H
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
char* ulong_to_hexstr(uint32_t num){
    char* buf;
    asprintf(&buf, "%x", (unsigned int)num);

    //make hexstr uppercase
    int i=0;
    while(buf[i]){
        buf[i] = toupper(buf[i]);
        i++;
    }
    return buf;
}

#endif
