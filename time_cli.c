/*
* @Author: Yinlong Su
* @Date:   2015-09-17 12:58:01
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-09-30 20:08:04
*
* File:         time_cli.c
* Description:  TIME Client C file
*/

#include "echotime.h"

int pipefd; // pipe file descriptor passed from client parent

/* --------------------------------------------------------------------------
 *  main
 *
 *  Entry function
 *
 *  @param  : int   argc
 *            char  **argv
 *  @return : int
 *  @see    : cli_time
 *  @usage  : ./time_cli <Server IP Address> <Pipe file descriptor>
 *  @warning: time_cli should be executed by client program, not by user
 *
 *  Process:
 *    01. Parse the argument to server address and pipe file descriptor;
 *    02. Connect to the server;
 *    03. Call cli_time to handle the communication.
 * --------------------------------------------------------------------------
 */
int main(int argc, char **argv) {
    int                 sockfd;
    struct sockaddr_in  servaddr;
    char                line[TIME_BUFFSIZE];

    pipefd = atoi(argv[2]);
    sockfd = Socket(AF_INET, SOCK_STREAM, 0);

    Dup2(pipefd, fileno(stderr));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT_TIME);
    Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    Connect(sockfd, (SA *)&servaddr, sizeof(servaddr));

    // Service startup message, print in both parent and child window
    snprintf(line, TIME_BUFFSIZE, "Time Service [%s:%d] @ pipe[%d]\n", argv[1], PORT_TIME, pipefd);
    Fputs(line, stdout);
    Writen(pipefd, line, strlen(line)+1);

    cli_time(sockfd);

    exit(0);
}

/* --------------------------------------------------------------------------
 *  cli_time
 *
 *  TIME Client function
 *
 *  @param  : int sockfd
 *  @return : void
 *
 *  Handle all the process between time client child process and the server
 * --------------------------------------------------------------------------
 */
void cli_time(int sockfd) {
    char    recvline[TIME_BUFFSIZE];

    // just use _Readline to get the message from socket
    while (Readline(sockfd, recvline, TIME_BUFFSIZE) > 0) {
        // print in both parent and child window
        Fputs(recvline, stdout);
        Writen(pipefd, recvline, strlen(recvline)+1);
    }
    err_quit("cli_time: server terminated prematurely");

}
