#include "udp_sockets.h"
                                                                                    
struct addrinfo* get_udp_sockaddr(const char* node, const char* port, int flags)
{
  struct addrinfo hints;
  struct addrinfo* results;
  int retval;

  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_family = AF_INET;      // Return socket addresses for our local IPv4 addresses
  hints.ai_socktype = SOCK_DGRAM; // Return UDP socket addresses                                   
  hints.ai_flags = flags;         // Socket addresses should be listening sockets                     
                         
  retval = getaddrinfo(node, port, &hints, &results);

  if (retval != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retval));
    exit(EXIT_FAILURE);
  }

  return results;
}

message* create_message()
{
  return (message*)malloc(sizeof(message));                                                   
}
                                                               
message* receive_message(int sockfd, host* source)
{
  message* msg = create_message();                                                                          

  // Length of the remote IP structure
  source->addr_len = sizeof(source->addr);

  // Read message, storing its contents in msg->buffer, and
  // the source address in source->addr
  msg->length = recvfrom(sockfd, msg->buffer, sizeof(msg->buffer), 0,                  
                         (struct sockaddr*)&source->addr,                           
                         &source->addr_len);

  // If a message was read
  if (msg->length > 0)
  {
    // Convert the source address to a human-readable form,
    // storing it in source->friendly_ip
    inet_ntop(source->addr.sin_family, &source->addr.sin_addr,                  
              source->friendly_ip, sizeof(source->friendly_ip));                  

    // Return the message received
    return msg;                                                               
  }
  else
  {
    // Otherwise, free the allocated memory and return NULL
    free(msg);                                                               
    return NULL;                                                               
  }
}

int send_message(int sockfd, message* msg, host* dest)
{
    return sendto(sockfd, msg->buffer, msg->length, 0,
                (struct sockaddr*)&dest->addr, dest->addr_len);
}

message* send_until_valid_ack(uint8_t seq, message* msg, int sockfd, host* connected_to, int timeout){
    message* response;
    int poll_ret = 0;

    //poll sockfd for POLLIN event
    struct pollfd fd = {
	.fd = sockfd,
	.events = POLLIN
    };
    
    do{
	//send the message until there is a response waiting
	while(poll_ret == 0){
	    if(send_message(sockfd, msg, connected_to)==-1){
		syslog(LOG_DEBUG, "Error sending message"); 
		exit(EXIT_FAILURE);
	    }
	    poll_ret = poll(&fd, 1, timeout);
	}
	poll_ret = 0;
	response = receive_message(sockfd, connected_to);

    }while(response->buffer[1] != seq); //until the message type matches seq
    

    return response;
}

