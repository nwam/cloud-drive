#ifndef HFTPD_H
#define HFTPD_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#include "../common/socketutils.h"
#include "../common/udp_server.h"
#include "../common/udp_sockets.h"
#include "../common/hftp_messages.h"
#include "../hdb/hdb.h"
#include "../common/termination_handler.h"
#include "../common/checksum_utils.h"

message* create_response_message(uint8_t, uint16_t); 
FILE* open_file(char*, char*, char*);
int strtoi(char* str, char* strerr);


#endif //HFTPD_H
