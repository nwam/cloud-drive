//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
// FILE:   hmds.c                                                     //
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


#include "hmds.h"

/*  binds hmds to a socket created from addr_list
    returns the file descriptor for the socket*/
int bind_socket(struct addrinfo* addr_list){

    struct addrinfo* addr;
    int sockfd;
    char yes = '1';

    //iterate over addresses until we successfully bind to one
    for(addr = addr_list; addr != NULL; addr= addr->ai_next){

        //open a socket
        sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if(sockfd==-1) continue;

        // Allow the port to be re-used if currently in the TIME_WAIT state
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
            err(EXIT_FAILURE, "%s", "Unable to set socket option");

        //bind socket to address/port
        if (bind(sockfd, addr->ai_addr, addr->ai_addrlen) == -1){
            //bind failed
            close(sockfd);
            continue;
        }
        else{
            //bind successful
            break;
        }
    }


    //if addr==NULL, we were unable to bind for every address
    if(addr==NULL){
        freeaddrinfo(addr_list);
        err(EXIT_FAILURE, "%s", "Unable to bind");
    }

    freeaddrinfo(addr_list);
    return sockfd;
}

/*  waits for an incomming connection to sockfd and establishes connection
    returns file descriptor for connection */
int wait_for_connection(int sockfd){
    struct sockaddr_in client_addr;            // Remote IP that is connecting to us
    unsigned int addr_len = sizeof(struct sockaddr_in); // Length of the remote IP structure
    char ip_address[INET_ADDRSTRLEN];          // Buffer to store human-friendly IP address
    int connectionfd;                          // Socket file descriptor for the new connection

    // Wait for a new connection
    connectionfd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_len);
    if (connectionfd == -1){
        syslog(LOG_ERR, "Unable to accept connection");
        exit(EXIT_FAILURE);
    }

    // print IP
    inet_ntop(client_addr.sin_family, &client_addr.sin_addr, ip_address, sizeof(ip_address));
    syslog(LOG_INFO, "Connection accepted from %s\n", ip_address);

    // Return the socket id for the new connection
    return connectionfd;

}

/*  waits for incomming messages, receives the message, and respondes accordingly*/
void handle_connection(int connectionfd, char* hostname){
    char* cur_req; //holds the current request
    char* username = "";

    //loop until the client disconnects or an error occurs
    do{
        //receive a request
        cur_req = recv_message(connectionfd);
        if(cur_req == NULL) break;

        //handle the request
        username = handle_request(connectionfd, cur_req, hostname, username);
    }while(!terminate);

    close(connectionfd);

}

/*  reads the type of request, and handles request accordingly */
char* handle_request(int connectionfd, char* request, char* hostname, char* username){

    char* type;
    int i=0;
    hdb_connection* con;

    //connect to the Redis server
    con = hdb_connect(hostname);

    //get the request type
    type = readuntil(request, '\n', &i);

    //AUTH request handling
    if(strcmp(type, "AUTH")==0){
        username = handle_auth(connectionfd, con, request, i);
        return username;

    //LIST request handling
    }else if(strcmp(type, "LIST")==0){
        handle_list(connectionfd, con, username, request, i);
        return username;
    }

    return "";

}

/*  handles the AUTH request:
    reads the username and password from the request,
    attempts to authenticate user in the Redis server,
    respondes to request according to authentication status
    returns the username*/
char* handle_auth(int connectionfd, hdb_connection* con, char* request, int i){
    char* username;
    char* password;
    char* key;
    char* value;

    //get the username and password
    while(request[i] != '\n'){
        key = readuntil(request, ':', &i);
        value = readuntil(request, '\n', &i);

        if(strcmp(key, "Username")==0){
            username = value;
            syslog(LOG_INFO, "Username: %s", username);
        }
        if(strcmp(key, "Password")==0){
            password = value;
        }
    }

    //authenticate user and get token
    char* token = hdb_authenticate(con, username, password);
    syslog(LOG_DEBUG, "%s's token is %s", username, token);

    //generate response
    char* response;
    int response_size;
    if(token != NULL){
        syslog(LOG_DEBUG, "Authentication successful");
        response_size = asprintf(&response, "200 Authentication successful\nToken:%s\n\n", token);
    }else{
        syslog(LOG_INFO, "Unauthorized: password incorrect");
        response_size = asprintf(&response, "401 Unauthorized\n\n");
    }

    //send response
    if (send(connectionfd, response, response_size, 0) == -1){
        syslog(LOG_WARNING, "Unable to send data to client");
    }

    return username;
}

/*  handles the LIST request:
    gets the token and body length from the request
    verifies the token
    if valid, gets the body of the request
    creates a list of files which are new/updated
    sends that list to the client */
void handle_list(int connectionfd, hdb_connection* con, char* username, char* request, int i){
    char* token = "";
    int list_length = 0;
    char* token_user;
    char* key;
    char* value;

    //get the token and list length
    while(request[i] != '\n'){
        key = readuntil(request, ':', &i);
        value = readuntil(request, '\n', &i);

        if(strcmp(key, "Token")==0){
            token = value;
        }
        if(strcmp(key, "Length")==0){
            list_length = atoi(value);
        }
    }

    //verify token
    token_user = hdb_verify_token(con, token);

    //generate response//
    char* response;
    int response_size;
    if(token_user!=NULL && strcmp(username, token_user)==0){
        //token is valid. Get the list of changed/new files
        syslog(LOG_INFO, "Receiving file list");
        char* list = recv_message_len(connectionfd, list_length);
        syslog(LOG_DEBUG, "List received:\n%s", list);
        char* req_list = get_new_file_list(con, username, list);

        //compose response
        if(strcmp(req_list,"")!=0){
            syslog(LOG_INFO, "Requesting file(s)");
            syslog(LOG_DEBUG, "The following file(s) are being requested:\n%s", req_list);
            asprintf(&response, "302 Files requested\nLength:%d\n\n%s", strlen(req_list), req_list);
        }else{
            syslog(LOG_INFO, "No file requests. All files are up to date");
            asprintf(&response, "204 No files requested\n\n");
        }


    }else{
        //token is invalid
        syslog(LOG_INFO, "Unauthorized: bad token");
        response_size = asprintf(&response, "401 Unauthorized\n\n");
    }

    //send response
    if (send(connectionfd, response, response_size, 0) == -1){
        syslog(LOG_WARNING, "Unable to send data to client");
    }

}

/*  gets the list of files which are new or have been updated */
char* get_new_file_list(hdb_connection* con, char* username, char* list){
    char* req_list = "";
    char* filename;
    char* checksum;
    char* hdb_checksum;
    int i = 0;
    char c = 1;

    //loop until the end of list
    while(c){
        //get the filename and checksum
        filename = readuntil(list, '\n', &i);
        checksum = readuntil(list, '\n', &i);

        if(hdb_file_exists(con, username, filename)){
            //file exists, compare checksums
            hdb_checksum = hdb_file_checksum(con, username, filename);
            if(strcmp(checksum, hdb_checksum)!=0){
                //checksums are different, add file to request list
                asprintf(&req_list, "%s%s\n", req_list, filename);
            }
        }else{
            //file does not exist. add to request list
            asprintf(&req_list, "%s%s\n", req_list, filename);
        }

        free(filename);
        free(checksum);
        c = list[i];
    }

    //remove the last blank line in req_list (the last char is currently '\n')
    req_list[strlen(req_list)] = '\0';
    return req_list;

}

/*  this function is called when ctrl+c is pressed,
    it sets a flag which helps ths hmds clean up before terminating */
void termination_handler(int signal){
    syslog(LOG_INFO, "Connection terminated");
    terminate = true;
}


int main(int argc, char *argv[]){

    //install action for ctrl+c (SIGINT)
    struct sigaction new_action;
    new_action.sa_handler = termination_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGINT, &new_action, NULL);

    //connect to syslog server
    openlog("hmds", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);

    /* process arguments */

    //optional args, set to their defaults
    char* hostname = "localhost";
    char* port = "9000";
    int verbose_flag = 0;

    //create the array of long optional args
    struct option long_options[] =
    {
        {"verbose", no_argument,       &verbose_flag, 1},
        {"server",  required_argument, 0,            's'},
        {"port",    required_argument, 0,            'p'},
        {0,0,0,0}
    };

    int c;
    while(1){

        int option_index = 0;
        c = getopt_long(argc, argv, "vs:p:d:", long_options, &option_index);

        //if we've reached the end of the options, stop iterating
        if (c==-1) break;

        switch(c)
        {
            case 's':
                hostname = optarg;
                break;

            case 'p':
                //port = strtoi(optarg,argv[0]);
                port = optarg;
                break;

            case 'v':
                verbose_flag = 1;
                break;

            case '?':
                exit(EXIT_FAILURE);
                break;
        }

    }

    //set verbose
    setlogmask(LOG_UPTO(LOG_INFO));
    if(verbose_flag){
        setlogmask(LOG_UPTO(LOG_DEBUG));
    }


    /* wait for connection request and communicate with client */
    int sockfd, connectionfd;

    struct addrinfo* info = get_sockaddr(NULL, port);
    sockfd = bind_socket(info);
    if(listen(sockfd, BACKLOG)==-1) err(EXIT_FAILURE, "%s", "Unable to listen on socket");
    syslog(LOG_INFO, "Server listening on port %s", port);
    //maintain connection
    while(!terminate){
        connectionfd = wait_for_connection(sockfd);
        handle_connection(connectionfd, hostname);
    }

    close(connectionfd);
    close(sockfd);

    //disconnect syslog
    closelog();

    exit(EXIT_SUCCESS);
}
