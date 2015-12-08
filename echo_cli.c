/*
* @Author: Yinlong Su
* @Date:   2015-09-16 16:48:06
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-09-30 20:07:33
*
* File:         echo_cli.c
* Description:  ECHO Client C file
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
 *  @see    : cli_echo
 *  @usage  : ./echo_cli <Server IP Address> <Pipe file descriptor>
 *  @warning: echo_cli should be executed by client program, not by user
 *
 *  Process:
 *    01. Parse the argument to server address and pipe file descriptor;
 *    02. Connect to the server;
 *    03. Call cli_echo to handle the communication.
 * --------------------------------------------------------------------------
 */
int main(int argc, char **argv) {
    int                 sockfd;
    struct sockaddr_in  servaddr;
    char                line[ECHO_BUFFSIZE];

    pipefd = atoi(argv[2]);
    sockfd = Socket(AF_INET, SOCK_STREAM, 0);

    Dup2(pipefd, fileno(stderr));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT_ECHO);
    Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    Connect(sockfd, (SA *)&servaddr, sizeof(servaddr));

    // Service startup message, print in both parent and child window
    snprintf(line, ECHO_BUFFSIZE, "Echo Service [%s:%d] @ pipe[%d]\n", argv[1], PORT_ECHO, pipefd);
    Fputs(line, stdout);
    Writen(pipefd, line, strlen(line)+1);

    cli_echo(stdin, sockfd);

    exit(0);
}

/* --------------------------------------------------------------------------
 *  cli_echo
 *
 *  ECHO Client function
 *
 *  @param  : FILE* fp
 *            int   sockfd
 *  @return : void
 *
 *  Handle all the process between echo client child process and the server
 * --------------------------------------------------------------------------
 */
void cli_echo(FILE *fp, int sockfd) {
    int     maxfdp1, stdineof;
    fd_set  rset;
    char    sendline[ECHO_BUFFSIZE], recvline[ECHO_BUFFSIZE];

    FD_ZERO(&rset);
    // stdineof is used as flag that indicates the client finished all the requests
    // when stdineof = 1, there might still be some message sending from the server
    stdineof = 0;

    for ( ; ; ) {
        if (stdineof == 0)
            FD_SET(fileno(fp), &rset);
        FD_SET(sockfd, &rset);
        maxfdp1 = max(fileno(fp), sockfd) + 1;
        Select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(sockfd, &rset)) {
            // try to read from the socket
            if (Readline(sockfd, recvline, ECHO_BUFFSIZE) == 0)
                if (stdineof == 1)
                    return;
                else
                    err_quit("cli_echo: server terminated prematurely");
            printf("< ");
            // print in both parent and child window
            Fputs(recvline, stdout);
            Writen(pipefd, recvline, strlen(recvline)+1);
        }
        if (FD_ISSET(fileno(fp), &rset)) {
            // try to read from stdin
            if (Fgets(sendline, ECHO_BUFFSIZE, fp) == NULL) {
                // user type EOF, set stdineof as 1, and shutdown the write end of socket
                stdineof = 1;
                Shutdown(sockfd, SHUT_WR);
                FD_CLR(fileno(fp), &rset);
                continue;
            }
            // write the message to the socket
            Writen(sockfd, sendline, strlen(sendline));
        }
    }
}
