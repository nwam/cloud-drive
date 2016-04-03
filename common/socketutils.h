//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
// FILE:   socketutils.h                                              //
// AUTHOR: Noah Murad, CS3357, Department of Computer Science         //
//         University of Western Ontario, London, Ontario, Canada     //
// DATE:   Friday November 13th, 2015                                 //

/* DESCRIPTION: This file contains methods which aid TCP. Due to the 
        stream orientation of TCP, these functions provide an easy way
        to retreive messages, weather they are sent in fragments, in
        multiples, or just regularly. There are also other network
        oriented functions which help create connections    */

#ifndef SOCKETUTILS_H
#define SOCKETUTILS_H
#define _GNU_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <err.h>
#include <stdlib.h>
#include <syslog.h>

/*  gets the socket address info for a client or server
    to get a socket for a server, set hostname to NULL*/
struct addrinfo* get_sockaddr(const char* hostname, const char* port);

/* 	calls recv() until a end of message is found (ie \n\n) 
	returns such a message */
char* recv_message(int fd);

/* calls recv() until a message of length len is formed */
char* recv_message_len(int fd, int len);

/*  returns the index of the end of a message (ie "\n\n\")
    returns -1 otherwise 
    if flag is set, returns 0 if the first character is \n 
        (meaning that the previous buffer ended with a \n)*/
int find_eom(char* message, int message_len, int* flag);

/* reads a str until end. 
	returns what was read and 
	index i via pointer, which will be place after char end */
char* readuntil(char* str, char end, int* i);

#endif //SOCKETUTILS_H
