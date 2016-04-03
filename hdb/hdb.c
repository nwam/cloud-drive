//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
// FILE:   hdb.c                                                      //
// AUTHOR: Noah Murad, CS3357, Department of Computer Science         //
//         University of Western Ontario, London, Ontario, Canada     //
// DATE:   Friday October 13th, 2015                                  //
// DESCRIPTION: The Hooli DataBase (hdb) contains a set of tools which//
//      communicate with Redis, a data structure store.               //
//      hdb stores, retrieves, and manipulates the data which is      //
//      returned by the Hooli client.                                 //
//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#include "hdb.h"

//connect to the Redis server
hdb_connection* hdb_connect(const char* server) {

  redisContext *context;  //the Redis connection
  const char *hostname = server; //the IP address
  int port = 6379; //port for connection

  //connect to Redis
  struct timeval timeout = {1,500000}; //1.5 seconds
  context = redisConnectWithTimeout(hostname, port, timeout);


  //bad connection error handling
  if (context == NULL || context->err) {
    if(context) {
      printf("Connection error: %s\n", context->errstr);
      redisFree(context);
    } else {
      printf("Connection error: can't allocate redis context\n");
    }
    exit(EXIT_FAILURE);
  }

  //recturn the connection as an hdb_connection
  return *(hdb_connection**)&context;
}

//disconnect from the Redis server
void hdb_disconnect(hdb_connection* con) {
  redisFree(concast(con));
}

//stores recrod in the Redis server
void hdb_store_file(hdb_connection* con, hdb_record* record) {
    char* cmd; //the Redis command as a string
    asprintf(&cmd, "HSET %s %s %s", record->username, record->filename, record->checksum);
    redis_cmd_null(con, cmd);
    free(cmd);
}

//removes file from the Redis server
int hdb_remove_file(hdb_connection* con, const char* username, const char* filename) {
    char* cmd; //Redis command as a string
    asprintf(&cmd, "HDEL %s %s", username, filename);
    int removed = redis_cmd_int(con, cmd);
    free(cmd);
    return removed;
}

//returns the checksum of a file on the Redis server
//NOTE: allocated memory is not released in this function
char* hdb_file_checksum(hdb_connection* con, const char* username, const char* filename) {
    //return NULL if the file does not exist
    if(! hdb_file_exists(con, username, filename)) return NULL;

    char *cmd, //Redis command as a string
         *checksum;  //filename's checksum
    asprintf(&cmd, "HGET %s %s", username, filename);
    checksum = redis_cmd_str(con, cmd);
    free(cmd);
    return checksum;
}

//retruns the number of files uploaded to the Redis server under the name username
int hdb_file_count(hdb_connection* con, const char* username) {
    char *cmd; //Redis command
    asprintf(&cmd, "HLEN %s", username);
    int num_files = redis_cmd_int(con, cmd);
    free(cmd);
    return num_files;
}

//returns true if user username exists. False otherwise
bool hdb_user_exists(hdb_connection* con, const char* username) {
    char *cmd; //Redis command
    asprintf(&cmd, "HKEYS %s", username);
    int num_files = redis_cmd_arrsz(con, cmd);
    free(cmd);
    return num_files == 0 ? false : true;
}

//returns true if file filename exists for the user username. False otherwise
bool hdb_file_exists(hdb_connection* con, const char* username, const char* filename) {
    char *cmd; //Redis command
    asprintf(&cmd, "HEXISTS %s %s", username, filename);
    int file_exists = redis_cmd_int(con, cmd);
    free(cmd);
    return file_exists;
}

//returns the head of a linked list conataining all of the user username's files on the Redis server
hdb_record* hdb_user_files(hdb_connection* con, const char* username) {
    //if the user has no files stored, return NULL
    if(hdb_file_count(con, username)==0) return NULL;

    hdb_record *head, *record;
    head = malloc(sizeof(struct hdb_record)); //head of the linked list. Used as return

    //get an array conataining every filename and checksum
    redisReply *reply = redisCommand(concast(con), "HGETALL %s", username);

    int i;
    //loop through the array of filenames and checksums
    record = head; //start from the head of the linkd list
    for(i=0; i<reply->elements; i++){
        if(i%2 == 0){  //filenames are in even indexes
            //store the filename and username in the record
            asprintf(&(record->filename), "%s", reply->element[i]->str);
            asprintf(&(record->username), "%s", username);
        }else{  //checksums are in odd indexes
            //store the checksum in the record
            asprintf(&(record->checksum), "%s", reply->element[i]->str);


            if(i == reply->elements -1){
                //if this is the last element in the list, make the next element NULL
                record->next = NULL;
            }else{
                //go to the next recrod in the linked list
                record->next = malloc(sizeof(struct hdb_record));
                record = record->next;
            }
        }
    }

    freeReplyObject(reply);
    return head;


}

//free's the memory allocated from the list returned by hdb_user_files(
void hdb_free_result(hdb_record* record) {
   hdb_record *next = record; //start from the head of the list

    while(next!=NULL){
        //get the next item in the list
        next = record->next;
        //free all of the values in the struct, and the struct itself
        free(record->username);
        free(record->filename);
        free(record->checksum);
        free(record);
        //continue to the next item
        record = next;
    }
}

/*authenticates a user by password, and exchanges his/her
username and password for a randomly-generated, 16-byte token that
will be passed by the client in subsequent requests made after authentication*/
char* hdb_authenticate(hdb_connection* con, const char* username, const char* password){
    int account_exists;
    char *cmd, *actual_password, *token;

    //check if the user exists
    asprintf(&cmd, "HEXISTS %s %s", PASS, username);
    account_exists = redis_cmd_int(con, cmd);

    if(account_exists){
    //account exists
        //compare the given password to the actual password
        asprintf(&cmd, "HGET %s %s", PASS, username);
        actual_password = redis_cmd_str(con, cmd);
        syslog(LOG_DEBUG, "Account exitsts! Password is %s", actual_password);

        //password correctness
        if(strcmp(actual_password, password)==0){
            //password is correct
            token = store_unique_token(con, username);
        }else{
            //password is incorrect
            return NULL;
        }

    //account does not exist
    }else{
        syslog(LOG_DEBUG, "Account does not exist. Creating new account");
        //store the new username, password, and token
        asprintf(&cmd, "HSET %s %s %s", PASS, username, password);
        redis_cmd_int(con,cmd);
        token = store_unique_token(con, username);
    }

    return token;
}

/*Verify the specified token, returning the username associated with the token,
 if it is a valid token, or NULL, if it is not valid*/
char* hdb_verify_token(hdb_connection* con, const char* token){
    char* u;
    if(hdb_file_exists(con, TOKEN, token)){
        u=get_token_user(con, token);
        return u;
    }else{
        return NULL;
    }

}

//removes user username and all of their files from the Redis server
int hdb_delete_user(hdb_connection* con, const char* username) {
    char* cmd;
    asprintf(&cmd, "DEL %s", username);
    int usr_deleted = redis_cmd_int(con, cmd);
    free(cmd);
    return usr_deleted;
}

//Helper function -- runs a Redis command cmd to Redis over the connection con
//This function is to be used with Redis commands which do not return values
void redis_cmd_null(hdb_connection *con, const char *cmd){
    redisReply *reply = redisCommand(concast(con), cmd);
    freeReplyObject(reply);
}

//Helper function -- runs a Redis command cmd to Redis over the connection con
//This function retruns the integer returned from the Redis reply
int redis_cmd_int(hdb_connection *con, const char *cmd){
    redisReply *reply = redisCommand(concast(con), cmd);
    int value = reply->integer;
    freeReplyObject(reply);
    return value;
}

//Helper function -- runs a Redis command cmd to Redis over the connection con
//This function retruns the array size returned from the Redis reply
int redis_cmd_arrsz(hdb_connection *con, const char *cmd){
    redisReply *reply = redisCommand(concast(con), cmd);
    int value = reply->elements;
    freeReplyObject(reply);
    return value;
}

//Helper function -- runs a Redis command cmd to Redis over the connection con
//This function retruns the string returned from the Redis reply
//NOTE: allocated memory is not released in this function
char* redis_cmd_str(hdb_connection *con, const char *cmd){
    redisReply *reply = redisCommand(concast(con), cmd);
    char *str;
    asprintf(&str, "%s", reply->str);
    freeReplyObject(reply);
    return str;
}

//randomly generates a 16-byte alphanumeric token
//NOTE: malloc in this function is never freed
char* generate_token(){
    int i;
    char* alphanum = ALPHANUM;

    //allocate memory for the token
    char* token = (char*)malloc(sizeof(char)*(TOKEN_SIZE+1));
    token[TOKEN_SIZE] = '\0';

    //generate the token
    srand(time(NULL));
    for(i=0; i<TOKEN_SIZE; i++){
        token[i] = alphanum[rand() % ALPHANUM_SIZE];
    }

    return token;
}

//stores a token, unique to all others, on Redis
//associates the stored token with the user username
//returns the token
//NOTE: some allocated memory is not freed in this function
char* store_unique_token(hdb_connection* con, const char* username){
    char* cmd;
    int token_exists = 1;
    char* token;

    while(1){
        //generate token and check for uniqueness
        token = generate_token();
        asprintf(&cmd, "HEXISTS %s %s", TOKEN, token);
        token_exists = redis_cmd_int(con, cmd);

        if(token_exists){
            //retry if token is not unique
            free(token);
        }else{
            //store the token if it is unique
            asprintf(&cmd, "HSET %s %s %s", TOKEN, token, username);
            redis_cmd_null(con, cmd);
            return token;
        }
    }
}

//returns the user associated with the specified token
char* get_token_user(hdb_connection* con, const char* token){
    return hdb_file_checksum(con, TOKEN, token);
}


//casts an hdb_connection* to a redisContext*
redisContext* concast(hdb_connection* con) {
    return *(redisContext**)&con;
}
