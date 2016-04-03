//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
// FILE:   socketutils.c                                              //
// AUTHOR: Noah Murad, CS3357, Department of Computer Science         //
//         University of Western Ontario, London, Ontario, Canada     //
// DATE:   Friday November 13th, 2015                                 //

/* DESCRIPTION: This file contains methods which aid TCP. Due to the 
        stream orientation of TCP, these functions provide an easy way
        to retreive messages, weather they are sent in fragments, in
        multiples, or just regularly. There are also other network
        oriented functions which help create connections    */


#include "socketutils.h"

/*  gets the socket address info for a client or server
    to get a socket for a server, set hostname to NULL*/
struct addrinfo* get_sockaddr(const char* hostname, const char* port){
    struct addrinfo hints; //hints passed to getaddrinfo
    struct addrinfo* results; //linked list of results from getaddrinfo

    //set hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          //IPv4 addresses
    hints.ai_socktype = SOCK_STREAM;    //TCP connection
    if(hostname==NULL)
	hints.ai_flags = AI_PASSIVE;    //socket for listening

    //resolve IP address (exit upon failure)
    int getaddrinfo_return = getaddrinfo(NULL, port, &hints, &results);
    if(getaddrinfo_return) errx(EXIT_FAILURE, "%s", gai_strerror(getaddrinfo_return));

    return results;
}

/*  calls recv() until a end of message is found (ie \n\n) 
    returns such a message */
char* recv_message(int fd){
    char* message = "";
    char buffer[4096];
    int bytes_read;
    int eom; //end of message
    int flag = 0;
    int message_size = 0;
    do
    {   
        //peek message and store to buffer
        bytes_read = recv(fd, buffer, sizeof(buffer) - 1, MSG_PEEK);

        if(bytes_read>0){
            eom = find_eom(buffer, bytes_read, &flag);
            
            if(eom){
                //end of message found!
                //add buffer and null character to message and return the message
                message_size+=eom;
                recv(fd, buffer, eom, 0);
                bytes_read = asprintf(&message, "%s%s", message, buffer);
                message[message_size] = '\0';
                return message;

            }else{
                //end of message not found
                //add buffer to message and keep on searching for the end of the message
                message_size += recv(fd, buffer, bytes_read, 0);
                asprintf(&message, "%s%s", message, buffer);
            }
        }

    } while (bytes_read > 0);

    return NULL;
}

/* calls recv() until a message of length len is formed */
char* recv_message_len(int fd, int len){
    char* msg = "";
    int msg_len = 0;
    char buffer[4096];
    int bytes_read;
    int bytes_to_read = sizeof(buffer) - 1 < len ? sizeof(buffer) - 1 : len;

    do{
        bytes_read = recv(fd, buffer, bytes_to_read, 0);
        asprintf(&msg, "%s%s", msg, buffer);
        msg_len += bytes_read;

    }while(msg_len<len && bytes_read > 0);

    //return the message
    if(msg_len!=len){
        free(msg);
        return NULL;
    }else{
        msg[len] = '\0';
        return msg;
    }

}

/*  returns the number of characters until the end of a message (ie "\n\n\")
    returns 0 otherwise 
    if flag is set, returns 1 if the first character is \n 
        (meaning that the previous buffer ended with a \n)*/
int find_eom(char* message, int message_len, int* flag){
    //check flag condition
    if(*flag && message[0]=='\n') return 1;

    *flag = 0;
    for(int i=0; i<message_len; i++){
        if(message[i]=='\n'){
            (*flag)++;
            if(*flag==2) return i+1;
        }else{
            *flag = 0;
        }
    }
    return 0;
}

/* reads a str until end. 
    returns what was read and 
    index i via pointer, which will be place after char end */
char* readuntil(char* str, char end, int* i){
    char* buf = "";
    char c;

    for(c=str[*i]; c!=end && c!='\0'; (*i)++, c=str[*i]){
        asprintf(&buf, "%s%c", buf, c);
    }

    if(c!='\0') (*i)++; //read past the specified character
    return buf;
}