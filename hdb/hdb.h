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

#ifndef HDB_H
#define HDB_H

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hiredis/hiredis.h>
#include <stdarg.h>
#include <time.h>
#include <syslog.h>
#include <limits.h>

#define PASS "password" //the hash under which passwords are stored on Redis
#define TOKEN "token" //the hash under which tokens are stored on Redis
#define STR_MAX 256

//constants for tokens
#define ALPHANUM "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
#define ALPHANUM_SIZE 62
#define TOKEN_SIZE 16

#define hdb_file_checksum get_token

// Connection to the Hooli database
typedef void* hdb_connection;

// A record stored in the Hooli database
typedef struct hdb_record {
  char* username;
  char* filename;
  char* checksum;
  char* abs_path;
  struct hdb_record* next;
} hdb_record;

// Connect to the specified Hooli database server, returning the initialized
// connection.
hdb_connection* hdb_connect(const char* db_server);

// Disconnect from the Hooli database server.
void hdb_disconnect(hdb_connection* con);

// Store a file record in the Hooli database.
void hdb_store_file(hdb_connection* con, hdb_record* record);

// Remove a file record from the Hooli database.
int hdb_remove_file(hdb_connection* con, const char* username, const char* filename);

// If the specified file is found in the Hooli database, return its checksum.
// Otherwise, return NULL.
char* hdb_file_checksum(hdb_connection* con, const char* username, const char* filename);

// Get the number of files stored in the Hooli database for the specified user.
int hdb_file_count(hdb_connection* con, const char* username);

// Return a Boolean value indicating whether or not the specified user exists
// in the Hooli database
bool hdb_user_exists(hdb_connection* con, const char* username);

// Return a Boolean value indicating whether or not the specified file exists
// in the Hooli database
bool hdb_file_exists(hdb_connection* con, const char* username, const char* filename);

// Return a linked list of all of the specified user's file records stored in the
// Hooli database
hdb_record* hdb_user_files(hdb_connection* con, const char* username);

// Free the linked list returned by hdb_user_files()
void hdb_free_result(hdb_record* record);

/*authenticates a user by password, and exchanges his/her
username and password for a randomly-generated, 16-byte token that
will be passed by the client in subsequent requests made after authentication*/
char* hdb_authenticate(hdb_connection* con, const char* username, const char* password);

/*Verify the specified token, returning the username associated with the token,
 if it is a valid token, or NULL, if it is not valid*/
char* hdb_verify_token(hdb_connection* con, const char* token);

// Delete the specifid user and all of his/her file records from the Hooli
// database.
int hdb_delete_user(hdb_connection* con, const char* username);

//casts an hdb_connection* to a redisContext*
redisContext* concast(hdb_connection* con);

//Helper function -- runs a Redis command cmd to Redis over the connection con
//This function is to be used with Redis commands which do not return values
void redis_cmd_null(hdb_connection* con, const char *cmd);

//Helper function -- runs a Redis command cmd to Redis over the connection con
//This function retruns the integer returned from the Redis reply
int redis_cmd_int(hdb_connection* con, const char *cmd);

//Helper function -- runs a Redis command cmd to Redis over the connection con
//This function retruns the array size returned from the Redis reply
int redis_cmd_arrsz(hdb_connection *con, const char *cmd);

//Helper function -- runs a Redis command cmd to Redis over the connection con
//This function retruns the string returned from the Redis reply
//NOTE: allocated memory is not released in this function
char* redis_cmd_str(hdb_connection *con, const char *cmd);

//randomly generates a 16-byte alphanumeric token
char* generate_token();

//stores a token, unique to all others, on Redis
//associates the stored token with the user username
//returns the token
char* store_unique_token(hdb_connection*, const char* username);

//returns the user associated with the specified token
char* get_token_user(hdb_connection* con, const char* token);

#endif

