/*
* @Author: Yinlong Su
* @Date:   2015-09-16 21:45:40
* @Last Modified by:   Yinlong Su
* @Last Modified time: 2015-09-29 23:06:19
*
* File:         tcpechotimesrv.c
* Description:  Server C file
*/

#include "echotime.h"

/* --------------------------------------------------------------------------
 *  sig_pipe
 *
 *  SIGPIPE Signal Handler
 *
 *  @param  : int signo
 *  @return : void
 *
 *  Catch EPIPE and report the error process' id
 * --------------------------------------------------------------------------
 */
void sig_pipe(int signo) {
    pid_t       pid = getpid();
    pthread_t   tid = pthread_self();

    printf("SIGPIPE from pid %u tid %u\n", (unsigned int)pid, (unsigned int)tid);

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
 *  @see    : echoserv, timeserv
 *  @usage  : ./server [&]
 *
 *  Server entry function, listening to the service ports and creating
 *  threads to handle client requests.
 * --------------------------------------------------------------------------
 */
int main(int argc, char **argv) {
    const int   on = 1;
    int         listenechofd, listentimefd, *connfd, maxfdp1, flag, r;
    pthread_t   tid;
    socklen_t   clilen;
    fd_set      rset;
    struct sockaddr_in cliaddr, servaddr;

    // use function sig_pipe as SIGPIPE handler
    Signal(SIGPIPE, sig_pipe);

    listenechofd = Socket(AF_INET, SOCK_STREAM, 0);
    listentimefd = Socket(AF_INET, SOCK_STREAM, 0);

    // set both listening socket to nonblocking
    flag = Fcntl(listenechofd, F_GETFL, 0);
    Fcntl(listenechofd, F_SETFL, flag | FNDELAY);

    flag = Fcntl(listentimefd, F_GETFL, 0);
    Fcntl(listentimefd, F_SETFL, flag | FNDELAY);

    // turn on SO_REUSEADDR and SO_KEEPALIVE in socket option
    // then bind and listen
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    servaddr.sin_port = htons(PORT_ECHO);
    Setsockopt(listenechofd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    Setsockopt(listenechofd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    Bind(listenechofd, (SA *)&servaddr, sizeof(servaddr));
    Listen(listenechofd, LISTENQ);

    servaddr.sin_port = htons(PORT_TIME);
    Setsockopt(listentimefd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    Setsockopt(listentimefd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    Bind(listentimefd, (SA *)&servaddr, sizeof(servaddr));
    Listen(listentimefd, LISTENQ);

    FD_ZERO(&rset);
    maxfdp1 = max(listenechofd, listentimefd) + 1;

    // print out the server startup message
    printf("\n[SERVER] TCP EchoTime Server started.\n");
    printf("[SERVER]     Echo Service port=%d, fd=%d\n", PORT_ECHO, listenechofd);
    printf("[SERVER]     Time Service port=%d, fd=%d\n\n", PORT_TIME, listentimefd);

    for ( ; ; ) {
        // use select() to monitor both listening sockets
        FD_SET(listenechofd, &rset);
        FD_SET(listentimefd, &rset);

        // need to use select rather than Select provided by Steven
        // cos Steven's Select doesn't handle EINTR
        r = select(maxfdp1, &rset, NULL, NULL, NULL);

        // slow system call select() may be interrupted
        if (r == -1 && errno == EINTR)
            continue;

        if (FD_ISSET(listenechofd, &rset)) {
            clilen = sizeof(cliaddr);
            connfd = Malloc(sizeof(int));

            // Echo Service request, accept the connection
            // and create a new thread to handle the request
            *connfd = Accept(listenechofd, (SA *)&cliaddr, &clilen);
            Pthread_create(&tid, NULL, &echoserv, connfd);
        }
        else if (FD_ISSET(listentimefd, &rset)) {
            clilen = sizeof(cliaddr);
            connfd = Malloc(sizeof(int));

            // Time Service request, accept the connection
            // and create a new thread to handle the request
            *connfd = Accept(listentimefd, (SA *)&cliaddr, &clilen);
            Pthread_create(&tid, NULL, &timeserv, connfd);
        }
    }
    exit(0);
}

/* --------------------------------------------------------------------------
 *  echoserv
 *
 *  ECHO Service thread function
 *
 *  @param  : void* arg (connected socket file descriptor)
 *  @return : void*
 *  @see    : str_echo
 *
 *  Thread function to handle client requests, pass the socket fd to
 *  echo service function str_echo
 * --------------------------------------------------------------------------
 */
static void *echoserv(void *arg) {
    // convert the argument to int and free the space
    int connfd = *((int *)arg);
    free(arg);

    // detach the thread
    pthread_t tid = pthread_self();
    Pthread_detach(tid);

    printf("\n[SERVER] Echo Service connected (%u).\n", (unsigned int)tid);
    // call str_echo to handle the ECHO Service
    str_echo(connfd);
    Close(connfd);
    printf("\n[SERVER] Echo Service finished.\n");
    return (NULL);
}

/* --------------------------------------------------------------------------
 *  str_echo
 *
 *  ECHO Service function
 *
 *  @param  : int sockfd
 *  @return : void
 *
 *  Service function to perform standard ECHO Service defined in RFC862
 *  Use select() to monitor the socket status
 * --------------------------------------------------------------------------
 */
void str_echo(int sockfd) {
    ssize_t n;
    int     r;
    fd_set  eset;
    char    buf[ECHO_BUFFSIZE];

    FD_ZERO(&eset);

    for ( ; ; ) {
        FD_SET(sockfd, &eset);
        // need to use select rather than Select provided by Steven
        // cos Steven's Select doesn't handle EINTR
        r = select(sockfd+1, &eset, NULL, NULL, NULL);

        // slow system call select() may be interrupted
        if (r < 0 && errno == EINTR)
            continue;

        // use read rather than Read coz we don't want the server terminates when error occurs
        if ((n = read(sockfd, buf, ECHO_BUFFSIZE)) > 0)
            // send back whatever received
            Writen(sockfd, buf, n);

        if (n == -1) {
            printf("\n[SERVER] Client termination: socket read returned with value -1\n");
            err_ret("str_echo: read error"); // use return, do not terminate server
        }
        if (n == 0) {
            printf("\n[SERVER] Client termination: socket read returned with value 0\n");
            break;
        }
    }
}

/* --------------------------------------------------------------------------
 *  timeserv
 *
 *  TIME Service thread function
 *
 *  @param  : void* arg (connected socket file descriptor)
 *  @return : void*
 *  @see    : str_time
 *
 *  Thread function to handle client requests, pass the socket fd to
 *  time service function str_time
 * --------------------------------------------------------------------------
 */
static void *timeserv(void *arg) {
    // convert the argument to int and free the space
    int connfd = *((int *)arg);
    free(arg);

    // detach the thread
    pthread_t tid = pthread_self();
    Pthread_detach(tid);

    printf("\n[SERVER] Time Service connected (%u).\n", (unsigned int)tid);
    // call str_time to handle the TIME Service
    str_time(connfd);
    Close(connfd);
    printf("\n[SERVER] Time Service finished.\n");
    return (NULL);
}

/* --------------------------------------------------------------------------
 *  str_time
 *
 *  TIME Service function
 *
 *  @param  : int sockfd
 *  @return : void
 *
 *  Service function to perform modified DAYTIME Service defined in RFC867,
 *  sending the daytime string to the client every five seconds.
 *  Use select() as an alarm and to monitor the socket status
 * --------------------------------------------------------------------------
 */
void str_time(int sockfd) {
    ssize_t n;
    int     r;
    char    buf[TIME_BUFFSIZE];
    time_t  ticks;
    fd_set  rset;

    // Timeout = 5s
    struct timeval timeout;
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;

    FD_ZERO(&rset);

    for ( ; ; ) {
        FD_SET(sockfd, &rset);
        // need to use select rather than Select provided by Steven
        // cos Steven's Select doesn't handle EINTR
        r = select(sockfd+1, &rset, NULL, NULL, &timeout);

        // slow system call select() may be interrupted
        if (r == -1 && errno == EINTR)
            continue;

        // use read rather than Read coz we don't want the server terminates when error occurs
        n = read(sockfd, buf, ECHO_BUFFSIZE);

        if (n == 0) {
            printf("\n[SERVER] Client termination: socket read returned with value 0\n");
            break;
        }
        if (n == -1 && errno == 0) {
            // timeout and send the daytime string to the client
            ticks = time(NULL);
            snprintf(buf, TIME_BUFFSIZE, "%.24s\r\n", ctime(&ticks));
            Write(sockfd, buf, strlen(buf));
            continue;
        }
        if (n == -1) {
            printf("\n[SERVER] Client termination: socket read returned with value -1\n");
            err_ret("str_time: read error"); // do not terminate server
        }
    }
}
