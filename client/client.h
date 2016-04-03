//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
// FILE:   client.h                                                   //
// AUTHOR: Noah Murad, CS3357, Department of Computer Science         //
//         University of Western Ontario, London, Ontario, Canada     //
// DATE:   Friday November 13th, 2015                                 //
// DESCRIPTION: This is the client of the Hooli drive. It recursively //
//      iterates through the Hooli directory, collecting files and    //
// 		their checksums. It then sends the user's credentials to the  //
//		hmds. After the credentials have been accepted, client will   //
//		upload the files requested from the server					  //
//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#ifndef CLIENT_H
#define CLIENT_H
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <zlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <getopt.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/stat.h>

#include "../common/socketutils.h"
#include "../common/udp_client.h"
#include "../common/udp_sockets.h"
#include "../hdb/hdb.h"
#include "../common/hftp_messages.h"

#define POLL_TIME 10000

/* This function recursively iterates through directories starting at "dir".
   For each file, this algorithm prints it's relative path from "dir", along
   with its CRC-32 checksum, into the file "file"*/
void iterate_dirs(char*, char*, hdb_record*, char*);

/* computes the crc32 value of the given file */
unsigned long crc(char[]);

/* adds data to linked list. returns a pointer to the newly added node */
void add_to_list(hdb_record*, char*, char*, char*, char*);

/* returns the hdb_recrod in a linked list with a mathcing filename to parameter filename */
hdb_record* get_hdb_record(hdb_record* head, char* filename);

/* converts an unsigned long to a char */
char* ulong_to_hexstr(uLong num);

/* frees values associated with linked list of crc values */
void free_hdb_list(hdb_record* head);

/* expands a '~' in a directory path name to the home directory */
char* expand_home_dir(char* dir);

/* opens a socket for this process and connects to the server */
int open_connection(struct addrinfo* addr_list);

/* 	creates, sends, receives, and handles the response of the auth request
	returns the token generated for the current connection to the hmds*/
char* auth_request(int sockfd, char* username, char* password);

/*  handles the LIST request. creates the request, gets the response,
    and handles the response accordingly */
char* list_request(int sockfd, hdb_record*, char* token);

/*  creates the body of the LIST request using a linked list of hdb_records
     and returns the size of the body in bytes */
int create_list_body(hdb_record* head, char** list);

/* returns the size of the parameter file */
uint32_t filesize(char* file);

/*sends all the files in requested_files to fport at fserver under user token */
void send_files(char*, char*, char*, hdb_record*, char*, char*);

/* creates a control message using parameters as feilds */
message* compose_control_message(uint8_t type, uint8_t seq, hdb_record* file_record, uint16_t filename_len, char* token, char* abs_path);

/* creates a data message from file f, with seq seq. Returns the message and sets eof if the end of file has been reached*/
message* compose_data_message(FILE*, uint8_t, int*);

/* returns the filesize of file */
uint32_t filesize(char* file);

#endif /* CLIENT_H */
