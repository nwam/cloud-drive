//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
// FILE:   client.c                                                   //
// AUTHOR: Noah Murad, CS3357, Department of Computer Science         //
//         University of Western Ontario, London, Ontario, Canada     //
// DATE:   Friday November 13th, 2015                                 //
// DESCRIPTION: This is the client of the Hooli drive. It recursively //
//      iterates through the Hooli directory, collecting files and    //
//      their checksums. It then sends the user's credentials to the  //
//      hmds. After the credentials have been accepted, client will   //
//      upload the files requested from the server                    //
//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#include "client.h"


/* This function recursively iterates through directories starting at "dir".
   For each file, this algorithm prints it's relative path from "dir", along
   with its CRC-32 checksum, into the file "file"*/
void iterate_dirs(char* dir, char* relative_path, hdb_record* head, char* username){

    DIR* d; //the directory in which we are iterating

    d = opendir(dir);
    //check to see if the directory was succesfully opened
    if (!d) {
	syslog(LOG_ERR, "Cannot open directory '%s'\n", dir);
	exit(EXIT_FAILURE);
    }

    struct dirent* entry; //a file/directory in the directory "dir"
    //begin iterating through directories. Break when no more entries remain in dir
    while( (entry = readdir(d)) ) {
        //get the name and path of the entry
        char* entry_name;       //the name of "entry"
        char path[PATH_MAX];    //the path of "entry"
        entry_name = entry->d_name;
        sprintf(path, "%s/%s", dir, entry_name);


        //if the entry is a file...
        DIR* is_dir = opendir(path);
        if (! is_dir) {
            if(fopen(path,"r")){

                //add the file's path and crc-32 to "file" to a linked list
                char* path_of_file;
                asprintf(&path_of_file, "%s%s", relative_path, entry_name);
                syslog(LOG_DEBUG, " * found file: %s (%s)\n", path_of_file, ulong_to_hexstr(crc(path)));
                add_to_list(head, path, path_of_file, ulong_to_hexstr(crc(path)), username);
            }else{
                syslog(LOG_WARNING, "Could not open %s", path);
            }

        }


        //if the entry is a directory (which is not the parent dir or the dir itslef)
        else if(strcmp (entry_name, "..") != 0 && strcmp(entry_name, ".") != 0) {

            //save parent path
	        char* parent; //parent of "entry"
            asprintf(&parent, "%s", relative_path);

	        //update the relative path
            strcat(relative_path, entry_name);
            strcat(relative_path, "/");

            //recursively iterate through this directory
            iterate_dirs(path,relative_path, head, username);

	        //return the relative_dir back to parent
	        relative_path = strcpy(relative_path, parent);
	        free(parent);

        }
    }

    //close the directory
    if (closedir(d)) {
	    syslog(LOG_ERR, "Could not close '%s'\n", dir);
	    exit(EXIT_FAILURE);
    }

}

/* computes the crc32 value of the given file */
unsigned long crc(char filepath[PATH_MAX]){

    Bytef* buf; //a buffer to hold the contents of the file
    long len;  //the length of the file
    FILE* fp = fopen(filepath,"rb"); //a pointer to the file

    //open file and check for a successful open
    if(fp==NULL){
	   syslog(LOG_WARNING, "Could not open file '%s' to calculate checksum\n", filepath);
	   exit(EXIT_FAILURE);
    }

    //get the length of the file
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    rewind(fp);

    //get a binary buffer of the contents of the file
    buf = (Bytef*)malloc((len+1)*sizeof(Bytef));
    fread(buf, len, 1, fp);
    fclose(fp); //close the file

    //compute the crc32 value
    uLong crc_value = crc32(0L, Z_NULL, 0);
    crc_value = crc32(crc_value, buf, len);

    return crc_value;
}


/* adds data to linked list. returns a pointer to the last node */
void add_to_list(hdb_record* head, char* abs_path, char* filepath, char* crc32, char* uname){
    hdb_record* current = head;
    //find the first empty node
    while(current->next != NULL){
        current = current->next;
    }

    //inject data
    current->filename = filepath;
    current->checksum = crc32;
    current->username = uname;
    current->abs_path = abs_path;

    //open up next node
    current->next = malloc(sizeof(hdb_record));
    current->next->next = NULL;
}

/* returns the hdb_recrod in a linked list with a mathcing filename to parameter filename */
hdb_record* get_hdb_record(hdb_record* head, char* filename){
    hdb_record* current = head;
    //find the node containing the same filename as filename
    while(current != NULL){
        if(strcmp(current->filename, filename) == 0){
            return current;
        }
        current = current->next;
    }

    return NULL;
}


/* converts an unsigned long to a char */
char* ulong_to_hexstr(uLong num){
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


/* frees values associated with linked list of crc values */
void hdb_free_result(hdb_record* record) {
   hdb_record *next = record; //start from the head of the list

    while(next->next!=NULL){
        //get the next item in the list
        next = record->next;
        //free all of the values in the struct, and the struct itself
        //free(record->username);
        free(record->filename);
        free(record->checksum);
        free(record);
        //continue to the next item
        record = next;
    }

    free(record->next);
    free(record);
}

/* expands a '~' in a directory path name to the home directory */
char* expand_home_dir(char* dir){
    if(*dir == '~'){
        //remove the ~
        dir++;

        //append dir to the home dir
        asprintf(&dir, "%s%s", getpwuid(getuid())->pw_dir,dir);
    }

    return dir;
}


/* opens a socket for this process and connects to the server */
int open_connection(struct addrinfo* addr_list){
    struct addrinfo* addr;
    int sockfd;

    //connect to an address in addr_list
    for(addr = addr_list; addr != NULL; addr = addr->ai_next){

        //open a socket
        sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if(sockfd==-1) continue;

        //connect to server
        syslog(LOG_INFO, "Connecting to server");
        if(connect(sockfd, addr->ai_addr, addr->ai_addrlen) != -1)
            break;
    }

    //if addr==NULL, we were unable to connect
    if(addr==NULL){
        freeaddrinfo(addr_list);
        err(EXIT_FAILURE, "%s", "Unable to connect");
    }

    freeaddrinfo(addr_list);
    return sockfd;

}

/*  creates, sends, receives, and handles the response of the auth request
    returns the token generated for the current connection to the hmds*/
char* auth_request(int sockfd, char* username, char* password){

    char* auth_req;
    char* auth_rsp;
    char* status;
    int i = 0;

    //create and send the request
    asprintf(&auth_req, "AUTH\nUsername:%s\nPassword:%s\n\n", username, password);
    syslog(LOG_DEBUG, "Sending credentials");
    if(send(sockfd, auth_req, strlen(auth_req), 0) == -1){
        syslog(LOG_ERR, "%s", "Unable to send");
        exit(EXIT_FAILURE);
    }

    //get the response
    auth_rsp = recv_message(sockfd);

    //get the response status
    status = readuntil(auth_rsp, ' ', &i);
    readuntil(auth_rsp, '\n', &i);


    //auth successful
    if(strcmp(status, "200")==0){

        syslog(LOG_INFO, "Authentication successful");

        char* key;
        char* value;
        //get token from headers
        while(auth_rsp[i] != '\n'){
            key = readuntil(auth_rsp, ':', &i);
            value = readuntil(auth_rsp, '\n', &i);

            if(strcmp(key, "Token")==0){
                free(key);
                return value; //return token
            }else{
                free(key);
                free(value);
            }
        }


    //unauthorized
    }else{
        syslog(LOG_INFO, "Authentication unsuccessful");
    }

    return NULL;

}

/*  handles the LIST request. creates the request, gets the response,
    and handles the response accordingly */
char* list_request(int sockfd, hdb_record* head, char* token){

    char* list;
    int list_length;
    char* list_req;
    char* list_rsp;
    char* status;
    int i;

    list_length = create_list_body(head, &list);

    //create the LIST request
    asprintf(&list_req, "LIST\nToken:%s\nLength:%d\n\n%s", token, list_length, list);

    //send the request
    syslog(LOG_INFO, "Uploading file list");
    if(send(sockfd, list_req, strlen(list_req), 0) == -1){
        syslog(LOG_ERR, "%s", "Unable to send");
        exit(EXIT_FAILURE);
    }

    //get the response
    list_rsp = recv_message(sockfd);

    i=0;
    //get the response status
    status = readuntil(list_rsp, ' ', &i);
    readuntil(list_rsp, '\n', &i);

    if(strcmp(status, "204")==0){
        //no files requested
        syslog(LOG_INFO, "No files requested");
        return NULL;
    }else if(strcmp(status, "302")==0){
        //files requested, get and print list

        //get the list length
        int list_length = 0;
        while(list_rsp[i] != '\n'){
            char* key = readuntil(list_rsp, ':', &i);
            char* value = readuntil(list_rsp, '\n', &i);

            if(strcmp(key, "Length")==0){
                list_length = atoi(value);
            }
        }

        //get and return the list of requested files
        syslog(LOG_INFO, "Server is requesting file(s)");
        char* req_files = recv_message_len(sockfd, list_length);
        syslog(LOG_DEBUG, "Server requested the following file(s):\n%s", req_files);
        return req_files;

    }else if(strcmp(status, "401")){
        //token mismatch
        syslog(LOG_INFO, "Unauthorized: bad token");
        return NULL;
    }

    return NULL;

}

/*  creates the body of the LIST request using a linked list of hdb_records
     and returns the size of the body in bytes */
int create_list_body(hdb_record* head, char** list){
    *list = "";
    int length = 0;
    //loop through the linked list of hdb_record's
    for(hdb_record* current = head; current->next!=NULL; current = current->next){
        //add the filename to list
        length += strlen(current->filename) + 1;
        asprintf(list, "%s%s\n", *list, current->filename);
        //add the checksum to list
        length += strlen(current->checksum) + 1;
        asprintf(list, "%s%s\n", *list, current->checksum);
    }

    length--; //no '\n' on last line
    list[length] = '\0';
    return length;

}


void send_files(char* fserver, char* fport, char* requested_files, hdb_record* file_list, char* token, char* root_dir){
    //message related variable declarations/initilizations
    host server;                        //address of the hftpd server
    message* msg;                       //message to send
    response_message* response;         //the response returned by the server
    int sockfd;                         //the socket id connected to the hftpd
    uint8_t next_seq = 0;               //the next sequence number for RDT

    //file related variable declarations/initilizations
    int i=0;                            //index for reading files from list of requested files
    int i_prev =i;                      //index of last file read
    uint16_t filename_len;              //length of the current filename
    char* filename;                     //the current filename
    char* abs_path;			//absolute path of the file
    hdb_record* file_record;            //the hdb_record of the current file
    FILE* f;				//a pointer to the actual file

    //create a socket to communicate with hftpd
    sockfd = create_client_socket(fserver, fport, &server);

    while(1){

	//get the next filename
	filename = readuntil(requested_files, '\n', &i);
	filename_len = (uint16_t)(i-i_prev-1);
	i_prev = i;



	//get the absolute path of the file
	asprintf(&abs_path, "%s/%s", root_dir, filename);

	//break if there are no more files
	file_record = get_hdb_record(file_list, filename);
	if(file_record == NULL){
	    break;
	}

	//compose the init control message
	msg = compose_control_message(CONTROL_INIT, next_seq, file_record, filename_len, token, abs_path);

	//send the control message and receive a valid ack
	syslog(LOG_DEBUG, "Seding control init message");
	response = (response_message*)send_until_valid_ack(msg->buffer[1], msg, sockfd, &server, POLL_TIME);
	next_seq = (next_seq+1)%2;
	free(msg);
    
	syslog(LOG_DEBUG, "Sending %s", filename);

	//exit if there is an error
	if(response->err_code == AUTHENTICATION_ERROR){
	    syslog(LOG_ERR, "Token mismatch. Exiting");
	    exit(EXIT_FAILURE);
	}

	//open the file
	f = fopen(abs_path, "rb");
	int eof = 0;

	//send data messages until entire file is sent
	while(eof == 0){
	    //compose and send message
	    msg = compose_data_message(f, next_seq, &eof);
	    syslog(LOG_INFO, "Sending data for %s", filename);
	    response = (response_message*)send_until_valid_ack(msg->buffer[1], msg, sockfd, &server, POLL_TIME);
	    next_seq = (next_seq+1)%2;
	}
	free(msg);
    }

    syslog(LOG_INFO, "Done sending all files");
    //send a terminating control message
    msg = compose_control_message(CONTROL_TERM, next_seq, file_record, 0, token, abs_path);
    response = (response_message*)send_until_valid_ack(msg->buffer[1], msg, sockfd, &server, POLL_TIME);
    close(sockfd);


}


message* compose_control_message(uint8_t type, uint8_t seq, hdb_record* file_record, uint16_t filename_len, char* token, char* abs_path){
    control_message* msg = (control_message*)create_message();
    //set msg feilds
    msg->type		= type;
    msg->seq		= seq;
    msg->filename_len	= htons(filename_len);
    memcpy(msg->token, token, TOKEN_SIZE);
    msg->length		= CONTROL_STATIC_SIZE + filename_len;
    if(filename_len != 0){
	uint32_t crcval = crc(abs_path);
	msg->filesize	= htonl(filesize(abs_path));
	msg->checksum	= htonl(crcval);
	memcpy(msg->filename, file_record->filename, filename_len);
    }


    return (message*)msg;
}

message* compose_data_message(FILE* f, uint8_t seq, int* eof){
    data_message* msg = (data_message*)create_message();
    //set type and seq
    msg->type	     = DATA_TYPE;
    msg->seq	     = seq;
    
    //read the file to the message
    uint16_t bytes_read;
    uint8_t* data_ptr = msg->data;
    bytes_read = fread(data_ptr, sizeof(uint8_t), MAX_DATA_SIZE, f);
    msg->data_len = htons(bytes_read);

    //set eof if the end of file has been reached, exit if an error occured
    if(bytes_read < MAX_DATA_SIZE){
	if(feof(f)){
	    *eof = 1;
	}
	else if(ferror(f)){
	    syslog(LOG_ERR, "Error reading file");
	    exit(EXIT_FAILURE);
	}
    }
    
    msg->length = DATA_STATIC_SIZE + bytes_read;

    return (message*)msg;
}


uint32_t filesize(char* file){
    FILE * f = fopen(file, "r");
    fseek(f, 0, SEEK_END);
    uint32_t len = (unsigned long)ftell(f);
    fclose(f);
    return len;
}

int main(int argc, char *argv[]){

    openlog("client", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);
    int c;

    //required args
    char* username;
    char* password;

    //optional args, set to their defaults
    char* hostname = "localhost";
    char* port = "9000";
    char* root_dir = "~/hooli";
    char* fserver = "localhost";
    char* fport = "10000";
    int verbose_flag = 0;

    //create the array of long optional args
    struct option long_options[] =
    {
        {"verbose", no_argument,       &verbose_flag, 1 },
        {"server",  required_argument, 0,            's'},
        {"port",    required_argument, 0,            'p'},
        {"dir",     required_argument, 0,            'd'},
        {"fserver", required_argument, 0,            'f'},
        {"fport",   required_argument, 0,            'o'},
        {0,0,0,0}
    };

    /* process optional arguments */
    while(1){

        int option_index = 0;
        c = getopt_long(argc, argv, "vs:p:d:f:o:", long_options, &option_index);
        //if we've reached the end of the options, stop iterating
        if (c==-1) break;

        switch(c)
        {
            case 's':
                hostname = optarg;
                break;

            case 'p':
                port = optarg;
                break;

            case 'd':
                root_dir = optarg;
                break;

            case 'f':
                fserver= optarg;
                break;

            case 'o':
                fport = optarg;
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

    /* process required arguments */
    if(argc - optind == 2){
        username = argv[optind];
        password = argv[optind+1];
    }else{
        syslog(LOG_ERR, "%s: usage: %s [OPTIONS] username password\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);

    }



    //set up variables to communicate with the hmds server
    root_dir = expand_home_dir(root_dir);
    hdb_record *head = malloc(sizeof(hdb_record)); //head of linked list of files and checksums
    head->next = NULL;
    char r_path[PATH_MAX]= ""; //relative path from root_dir. used to print to file argv[2]

    //iterate the directories, starting from the root dir, gathering files and checksums
    syslog(LOG_INFO, "Scanning directory: %s", root_dir);
    iterate_dirs(root_dir, r_path, head, username);

    //connect to server and authorize user
    struct addrinfo* info = get_sockaddr(hostname, port);
    int sockfd = open_connection(info);
    char* token = auth_request(sockfd, username, password);

    //if the user was authenticated successfully
    if(token != NULL){

        //get the list of requested files from the hmds server
        char* requested_files = list_request(sockfd, head, token);

        //initiate a connection with hftpd server and send the requested files
        send_files(fserver, fport, requested_files, head, token, root_dir);

        //clean up
        hdb_free_result(head);

    }


    //clean up
    close(sockfd);
    closelog();

    exit(EXIT_SUCCESS);
}
