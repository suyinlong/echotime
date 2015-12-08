/*
* @Author: Yinlong Su
* @Date:   2015-09-16 11:33:52
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-09-30 20:48:39
*
* File:         tcpechotimecli.c
* Description:  Client C file
*/

#include "echotime.h"

/* --------------------------------------------------------------------------
 *  nonblock
 *
 *  Keyboard mode change function
 *
 *  @param  : int state (as NB_ENABLE or NB_DISABLE)
 *  @return : void
 *
 *  When user interactives with client's menu, I want the stdin can response
 *  to a signle char immediately without an ENTER. So this function can turn
 *  on/off the keyboard canonical mode and set the minimum number of input
 *  read to 1.
 * --------------------------------------------------------------------------
 */
void nonblock(int state) {
    struct termios ttystate;

    // get and save the terminal state
    tcgetattr(STDIN_FILENO, &ttystate);
    if (state == NB_ENABLE) {
        // turn off canonical mode
        ttystate.c_lflag &= ~ICANON;
        // minimum of number input read
        ttystate.c_cc[VMIN] = 1;
    }
    else if (state == NB_DISABLE) {
        // turn ou canonical mode
        ttystate.c_lflag |= ICANON;
    }
    // set the terminal state
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

/* --------------------------------------------------------------------------
 *  sig_chld
 *
 *  SIGCHLD Signal Handler
 *
 *  @param  : int signo
 *  @return : void
 *
 *  Catch SIGCHLD signal and terminate all the zombie children
 * --------------------------------------------------------------------------
 */
void sig_chld(int signo) {
    pid_t   pid;
    int     stat;

    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
        printf("\nChild %d terminated.\n", pid);
    return;
}

/* --------------------------------------------------------------------------
 *  sig_int
 *
 *  SIGINT Signal Handler
 *
 *  @param  : int signo
 *  @return : void
 *
 *  Catch SIGINT signal when user types Ctrl+C in the parent window, this
 *  function will close all the child processes by sending SIGKILL
 * --------------------------------------------------------------------------
 */
void sig_int(int signo) {
    kill(0, SIGKILL);
    return;
}

/* --------------------------------------------------------------------------
 *  main
 *
 *  Entry function
 *
 *  @param  : int   argc
 *            char  **argv
 *  @return : int
 *  @see    : nonblock, echo_cli.c, time_cli.c
 *  @usage  : ./client <Server IP address or domain name>
 *
 *  Process:
 *    01. Parse the argument to server address;
 *    02. Register signal handler and turn off the keyboard canonical mode;
 *    03. Show the services menu;
 *    04. If a specific service is chosen in 03, fork a child process and use
          pipe to communicate;
 *    05. If the service is terminated by user, return to 03 until user
          chooses quit;
 *    06. Turn on the keyboard canonical mode and quit.
 * --------------------------------------------------------------------------
 */
int main(int argc, char **argv)
{
    pid_t               childpid;
    int                 stat, pfd[2], c, r, i;
    char                buf[PIPE_BUFFSIZE], pipe_str[PIPESTR_BUFFSIZE], ipaddr[IP_BUFFSIZE];
    struct sockaddr_in  servaddr;
    struct hostent      *he;

    // command arguments error
    if (argc != 2)
        err_quit("usage: tcpcli <Server IP address or Domain Name>");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;

    int t = inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    if (t > 0) {
        // user input an IP adress, convert it to the domain name
        he = gethostbyaddr(&servaddr.sin_addr, sizeof(servaddr.sin_addr), AF_INET);
        if (he == NULL) {
            err_quit("gethostbyaddr error for %s", argv[1]);
            exit(1);
        }
        printf("Server host name: %s\n", he->h_name);
        for (i = 0; he->h_aliases[i] != NULL; i++) {
            printf("       alias name: %s\n", he->h_aliases[i]);
        }
        printf("Server IP address: %s\n", argv[1]);
        strncpy(ipaddr, argv[1], sizeof(ipaddr));
    }
    else {
        // user input a domain name, convert it to the IP address
        he = gethostbyname(argv[1]);
        if (he == NULL) {
            err_quit("gethostbyname error for %s", argv[1]);
            exit(1);
        }
        printf("Server host name: %s\n", argv[1]);
        printf("Server IP address: %s\n", inet_ntoa(*(struct in_addr*)he->h_addr));
        strncpy(ipaddr, inet_ntoa(*(struct in_addr*)he->h_addr), sizeof(ipaddr));
    }

    // use function sig_chld as SIGCHLD handler, function sig_int as SIGINT handler
    Signal(SIGCHLD, sig_chld);
    Signal(SIGINT, sig_int);

    // turn off the keyboard canonical mode
    nonblock(NB_ENABLE);
    for ( ; ; ) {
        // print the menu
        printf("\n= TCP EchoTime Client =\n");
        printf("1. Echo Service\n");
        printf("2. Time Service\n");
        printf("3. Quit\n");
        printf("\nPlease indicate your choice: > ");
        // read the user input from stdin
        c = fgetc(stdin);

        if (c == '3') break;
        if (c == '1' || c == '2') {
            // create the pipe and then fork
            Pipe(pfd);
            childpid = Fork();

            if (childpid == 0) {
                // this is child process part
                // close the read end of the pipe
                close(pfd[0]);
                snprintf(pipe_str, PIPESTR_BUFFSIZE, "%d", pfd[1]);
                if (c == '1') {
                    // Echo service, run ./echo_cli, and pass the server ipaddress and pipe file descriptor
                    printf("\nConnecting to Echo Service...\n");
                    if ((execlp("xterm", "xterm", "-e", "./echo_cli", ipaddr, pipe_str, (char *) 0)) < 0) {
                        printf("xterm start error!\n");
                        exit(1);
                    }
                }
                else if (c == '2') {
                    // Time service, run ./time_cli, and pass the server ipaddress and pipe file descriptor
                    printf("\nConnecting to Time Service...\n");
                    if ((execlp("xterm", "xterm", "-e", "./time_cli", ipaddr, pipe_str, (char *) 0)) < 0) {
                        printf("xterm start error!\n");
                        exit(1);
                    }
                }
            } else {
                // this is parent process part
                // close the write end of the pipe
                close(pfd[1]);

                for ( ; ; ) {
                    // in the for-loop, use
                    fd_set fds;
                    FD_ZERO(&fds);
                    FD_SET(STDIN_FILENO, &fds);
                    FD_SET(pfd[0], &fds);

                    bzero(buf, sizeof(buf));

                    // need to use select rather than Select provided by Steven
                    // cos Steven's Select doesn't handle EINTR
                    r = select(pfd[0] + 1, &fds, NULL, NULL, NULL);

                    // slow system call select() may be interrupted
                    if (r == -1 && errno == EINTR) {
                        continue;
                    }

                    if (FD_ISSET(pfd[0], &fds)) {
                        if (Read(pfd[0], buf, PIPE_BUFFSIZE) != 0)
                            // the pipe is readable, read the message and print
                            printf(": %s", buf);
                        else
                            // the pipe is closed by child process, exits
                            break;
                    }
                    if (FD_ISSET(STDIN_FILENO, &fds)) {
                        // the stdin is readable, just read the char and discard it!
                        int u = fgetc(stdin);
                    }

                    if (r == -1) {
                        err_sys("select error");
                    }
                }
                // close the read end of the pipe
                close(pfd[0]);

            }

        } else {
            printf("\n\nSorry, wrong input.\n");
        }

    }
    // turn on the keyboard canonical mode
    nonblock(NB_DISABLE);
    printf("\nGoodbye!\n");
    exit(0);
}
