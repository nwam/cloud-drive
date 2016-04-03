//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
// FILE:   hmds.s                                                     //
// AUTHOR: Noah Murad, CS3357, Department of Computer Science         //
//         University of Western Ontario, London, Ontario, Canada     //
// DATE:   Friday November 13th, 2015                                 //

/* DESCRIPTION: The hmds responds to requests made by client
        an AUTH request will make hmds contact the Redis server to
        see if the user's given credentials match those of the Redis
        server. If they do match, the client can send a LIST request
        containing all the files in the clients Hooli directory
        and hmds will respond with a list of files which need to be 
        reuploaded, be it that they are new or changed                */
//********************************************************************/ 

#ifndef HMDS_H
#define HMDS_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

//socket programming
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

//IO
#include <getopt.h>
#include <syslog.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>


#include "../hdb/hdb.h"
#include "../common/socketutils.h"

#define BACKLOG 25

static bool terminate = false;

#define SOCK_TYPE(s) (s == SOCK_STREAM ? "Stream" : s == SOCK_DGRAM ? "Datagram" : \
                      s == SOCK_RAW ? "Raw" : "Other")

/*  binds hmds to a socket created from addr_list
    returns the file descriptor for the socket*/
int bind_socket(struct addrinfo* addr_list);

/*  waits for an incomming connection to sockfd and establishes connection
    returns file descriptor for connection */
int wait_for_connection(int sockfd);

/*  waits for incomming messages, receives the message, and respondes accordingly*/
void handle_connection(int connectionfd, char* hostname);

/*  reads the type of request, and handles request accordingly */
char* handle_request(int connectionfd, char* request, char* hostname, char* retval);

/*  handles the AUTH request
	reads the username and password from the request, 
    attempts to authenticate user in the Redis server,
    respondes to request according to authentication status */
char* handle_auth(int connectionfd, hdb_connection* con, char* request, int i);

/*  handles the LIST request:
    gets the token and body length from the request
    verifies the token
    if valid, gets the body of the request
    creates a list of files which are new/updated
    sends that list to the client */ 
void handle_list(int connectionfd, hdb_connection* con, char* username, char* request, int i);

/*  gets the list of files which are new or have been updated */
char* get_new_file_list(hdb_connection* con, char* username, char* list);

/*  this function is called when ctrl+c is pressed,
    it sets a flag which helps ths hmds clean up before terminating */
void termination_handler(int signal);

#endif //HMDS_H
