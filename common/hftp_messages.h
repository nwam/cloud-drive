#ifndef CLIENT_MESSAGES_H
#define CLIENT_MESSAGES_H

#include <stdint.h>

#define TOKEN_SIZE 16
#define MAX_DATA_SIZE 1468 
#define MAX_FILENAME_SIZE 1444
#define RESPONSE_PADDING 1468


#define CONTROL_INIT 1
#define CONTROL_TERM 2
#define CONTROL_STATIC_SIZE 28

#define DATA_TYPE 3
#define DATA_STATIC_SIZE 4

#define RESPONSE_TYPE 255
#define AUTHENTICATION_ERROR 1
#define RESPONSE_LENGTH 4
#define ACK 0


typedef struct
{
   int length;
   uint8_t type;
   uint8_t seq;
   uint16_t filename_len;
   uint32_t filesize;
   uint32_t checksum;
   uint8_t token[TOKEN_SIZE];
   uint8_t filename[MAX_FILENAME_SIZE];
} control_message;

typedef struct
{
   int length;
   uint8_t type;
   uint8_t seq;
   uint16_t data_len;
   uint8_t data[MAX_DATA_SIZE];
} data_message;

typedef struct
{
    int length;
    uint8_t type;
    uint8_t seq;
    uint16_t err_code;
    uint8_t padding[RESPONSE_PADDING];
} response_message;

#endif
