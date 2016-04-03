#ifndef TERMINATION_HANDLER_H
#define TERMINATION_HANDLER_H

#include <signal.h>

static bool terminate = false;

//handles SIGINT action
void termination_handler(int signal){
    terminate = true;
}

//installs actionf for SIGINT
void install_termination_handler(){
    struct sigaction new_action;
    new_action.sa_handler = termination_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGINT, &new_action, NULL);
}

#endif
