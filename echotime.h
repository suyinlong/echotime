#ifndef __echotime_h
#define __echotime_h

#include <sys/file.h>
#include <termios.h>
#include "unpthread.h"

// Port definition

#define PORT_ECHO   61173
#define PORT_TIME   61174

// Buffer size definition

#define IP_BUFFSIZE         20
#define PIPESTR_BUFFSIZE    10
#define PIPE_BUFFSIZE       1024
#define ECHO_BUFFSIZE       1024
#define TIME_BUFFSIZE       1024

// Keyboard nonblocking constants

#define NB_ENABLE  0
#define NB_DISABLE 1


// function headers

static void *echoserv(void *arg);
static void *timeserv(void *arg);

void str_echo(int);
void str_time(int);

void cli_echo(FILE*, int);
void cli_time(int);

#endif
