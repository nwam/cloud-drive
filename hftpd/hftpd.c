#include "hftpd.h"


/* creates and returns a response message (ACK) with sequence seq, and error code error) */
message* create_response_message(uint8_t seq, uint16_t error){
   //create a response message and initialize it
   response_message* response = (response_message*)create_message();
   response->type = RESPONSE_TYPE;
   response->seq  = seq;
   response->err_code = htons(error);
   response->length = RESPONSE_LENGTH;

   return (message*)response;
}

/* creates unexistant dirs, erases the files, then opens the file stored at root_dir/username/filename */
FILE* open_file(char* root_dir, char* username, char* filename){
	//get the filepath
	char* file_location;
	asprintf(&file_location, "%s/%s/%s", root_dir, username, filename);

	//create necessary dirs
	char* mkdirp;
	asprintf(&mkdirp, "mkdir -p `dirname %s`", file_location);
	system(mkdirp);
	free(mkdirp);

	//erase the file
	FILE* f = fopen(file_location, "w");
	fclose(f);

	//open the file
	f = fopen(file_location, "ab");
	free(file_location);

	return f;
}

/* converts a string to an integer, exits program if string is not an integer */
int strtoi(char* str, char* strerr){
    char* strp = str;
    long l;
    int i;

    l = strtol(str, &strp, 10);
    i = (int)l;

    //check if the input had additional characters after the int
    if(*strp != '\0'){
        syslog(LOG_ERR, "%s: int required", strerr);
        exit(EXIT_FAILURE);
    }

    return i;
}

int main(int argc, char *argv[]){

    openlog("hftpd", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);
    int c;

    //optional args, set to their defaults
    char* port = "10000";
    char* redis_hostname = "localhost";
    char* root_dir = "/tmp/hftpd";
    int timewait = 10;
    int verbose_flag = 0;

    //create the array of long optional args
    struct option long_options[] =
    {
        {"verbose",  no_argument,       &verbose_flag, 1 },
        {"port",     required_argument, 0,            'p'},
        {"redis",    required_argument, 0,            'r'},
        {"dir",      required_argument, 0,            'd'},
        {"timewait", required_argument, 0,            't'},
        {0,0,0,0}
    };

    /* process optional arguments */
    while(1){

        int option_index = 0;
        c = getopt_long(argc, argv, "p:r:d:t:v", long_options, &option_index);
        //if we've reached the end of the options, stop iterating
        if (c==-1) break;

        switch(c)
        {
            case 'p':
                port = optarg;
                break;

            case 'r':
                redis_hostname = optarg;
                break;

            case 'd':
                root_dir = optarg;
                break;

            case 't':
                timewait = strtoi(optarg, "-t / --timewait");
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

    //set up variables for the main program loop
    control_message* request;		//message received
    data_message* data;			//data message received
    message* response;			//message to send
    host client;			//client of this server
    int sockfd;				//the socket id of this server
    uint8_t expected_seq = 0;		//expected seq value of next message
    FILE* file = NULL;	    		//file that we are writing to
    char* username;			//username of whos uploading the files
    char* filename;
    char* checksum;
    char* token;

    //set up a udp socket to listen
    sockfd = create_server_socket(port);
    syslog(LOG_INFO, "Listening on port %s", port);

    //set up a connection with the redis server
    hdb_connection* redis_connection = hdb_connect(redis_hostname);
    
    while(!terminate){
	//wait for a type 1 control message
	syslog(LOG_DEBUG, "Waiting for control init message");
	request = (control_message*)receive_message(sockfd, &client);
	while(request->type != CONTROL_INIT){
	    request = (control_message*)receive_message(sockfd, &client);
	}
	syslog(LOG_DEBUG, "Control init message received");

	//main program loop
	while(request->type == CONTROL_INIT){

	    //get the token
	    token = (char*)malloc(sizeof(char)*TOKEN_SIZE + 1);
	    memcpy(token, request->token, TOKEN_SIZE);
	    token[TOKEN_SIZE] = '\0';

	    //get the username
	    username = get_token_user(redis_connection, token);
	    if(username == NULL){
		response = create_response_message(expected_seq, AUTHENTICATION_ERROR);
		send_message(sockfd, response, &client);
		free(request);
		continue;
	    }
	    free(token); 

	    //close the previous file and update the metadata
	    if(file != NULL){
		fclose(file);

		//update metadata
		hdb_record hdb_entry = {
		    .username = username,
		    .filename = filename, 
		    .checksum = checksum, 
		};
		hdb_store_file(redis_connection, &hdb_entry);
	    }


	    //get the filename and  filesize from the request
	    uint32_t filesize     = ntohl(request->filesize);
	    uint16_t filename_len = ntohs(request->filename_len);
	    filename = (char*)malloc(sizeof(char*)*filename_len + 1);
	    memcpy(filename, request->filename, filename_len);
	    checksum = ulong_to_hexstr(ntohl(request->checksum));
	    filename[filename_len] = '\0';

	    syslog(LOG_DEBUG, "Writing file %s", filename);

	    //open the file
	    file = open_file(root_dir, username, filename); 

	    //send an ack
	    response = create_response_message(expected_seq, ACK);
	    syslog(LOG_DEBUG, "Sending ack for control init. Seq %d", response->buffer[1]);
	    send_message(sockfd, response, &client);
	    expected_seq = (expected_seq+1)%2;

	    //get the first data message
	    syslog(LOG_DEBUG, "Waiting for first data message. Seq %d", expected_seq);
	    data = (data_message*)receive_message(sockfd, &client);

	    //assemble the file's data
	    int bytes_recvd = 0;
	    syslog(LOG_INFO, "Transferring file %s", filename); 
	    while(data->type == DATA_TYPE){
	       
		//sned an ACK
		response = create_response_message(expected_seq, ACK);
		send_message(sockfd, response, &client);

		//assemble the file if the request seq was expected
		if(data->seq == expected_seq){
		    bytes_recvd+= data->data_len;
		    syslog(LOG_DEBUG, "Successfully received data. Seq %d. File %s. %d/%d bytes received. %f percent complete ", expected_seq, filename,bytes_recvd, filesize, (float)bytes_recvd/(float)filesize );
		    expected_seq = (expected_seq+1)%2;
		    uint8_t* data_ptr = data->data;
		    fwrite(data_ptr, sizeof(uint8_t), ntohs(data->data_len), file);
		}

		//get the next request
		data  = (data_message*)receive_message(sockfd, &client);
	    }
	    

	    syslog(LOG_INFO, "File uploaded");
	    //get the next control message
	    request = (control_message*) data;
	    
	}

	//close the file
	fclose(file);

	//send a final ACK
	response = create_response_message(expected_seq, ACK);
	expected_seq = (expected_seq+1)%2;

	//set up a final poll to ensure the connection is closed
	struct pollfd pfd = {
	    .fd = sockfd,
	    .events = POLLIN
	};

	//run the poll until there are no messages for timewait
	syslog(LOG_INFO, "All files transferred, waiting for timeout to ensure client has terminated");
	int poll_ret = 0;
	do{
	    //send a message if the client hasnt terminated
	    if(poll_ret = 1){
		send_message(sockfd, response, &client);
	    }
	    poll_ret = poll(&pfd, 1, timewait);
	}while(poll_ret == 1);

	syslog(LOG_INFO, "Connection with %s closed", username);
	expected_seq = 0;   
	file = NULL; 
    }
    
    syslog(LOG_INFO, "Termination requested");
    //clean up
    close(sockfd);

}
