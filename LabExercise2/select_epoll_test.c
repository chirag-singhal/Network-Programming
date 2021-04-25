#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>

#define SERVER_PORT 3000
#define MAX_BUFF 256
#define MAX_EVENTS 30


void err_exit(const char * err_msg) {
    perror(err_msg);
    _exit(EXIT_FAILURE);
}

int max(int a, int b) {
    if(a > b)
        return a;
    return b;
}

void send_data(int N, int num_fd) {
    srand(getpid());

    struct timespec rand_time;
    rand_time.tv_sec = 0;
    rand_time.tv_nsec = rand();
    nanosleep(&rand_time, NULL);

    struct sockaddr_in serv_addr = {0};
    int conn_fd;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    void* buffer = malloc(MAX_BUFF);

    int fds[num_fd / N];

    for(int i = 0; i < num_fd / N; i++) {

        conn_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(conn_fd == -1) 
            err_exit("Error in socket. Exiting...\n");

        fds[i] = conn_fd;
        
        if (connect(conn_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
            err_exit("Error in connecting to server. Exiting...\n");
    }


    for(int i = 0; i < num_fd / N; i++) {

        memset(buffer, 0, MAX_BUFF);

        rand_time.tv_sec = 0;
        rand_time.tv_nsec = (rand() * 10 )/ num_fd;
        nanosleep(&rand_time, NULL);
        if(send(fds[i], buffer, MAX_BUFF, 0) == -1) 
            err_exit("Error in writing to server. Exiting....\n");
        close(fds[i]);
    }

    free(buffer);
}

void time_epoll(int N, int num_fd, int sockfd) {
    pid_t child_pid;

    for(int i = 0; i < N; i++) {
        child_pid = fork();
        if(child_pid == 0) {
            send_data(N, num_fd);
            _exit(EXIT_SUCCESS);
        }
        else if(child_pid < 0) {
            err_exit("Error in forking the server process. Exiting....");
        }
    }
    
    void* buffer = malloc(MAX_BUFF);
    num_fd = max(num_fd / N, 1) * N;

    struct sockaddr_in client;
    memset(&client, 0, sizeof(client));
    socklen_t client_len = sizeof(client);

    int epoll_fd = epoll_create1(0);
    if(epoll_fd == -1)
        err_exit("Error creating epoll. Exiting....\n");

    struct epoll_event epoll_events[MAX_EVENTS];
    int fds[num_fd];

    for(int i = 0; i < num_fd; i++) {
        fds[i] = accept(sockfd,(struct sockaddr*) &client, &client_len);
        if(fds[i] == -1) 
            err_exit("Error in accept in server. Exiting....\n");
        
        struct epoll_event event;
        memset(&event, 0, sizeof(event));
        event.events = EPOLLIN;
        event.data.fd = fds[i];
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fds[i], &event);
    }

    int num_of_recv = 0;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);

    for(; num_of_recv < num_fd; ){

       int event_count = epoll_wait(epoll_fd, epoll_events, MAX_EVENTS, 30000);
        
        for(int i = 0; i < event_count; i++) {
            memset(buffer, 0, MAX_BUFF);
            if(recv(epoll_events[i].data.fd, buffer, MAX_BUFF, 0) == -1)
                err_exit("Error in reading in server. Exiting....\n");
            num_of_recv++;
        }	
    }

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
    double time_diff = (end_time.tv_sec - start_time.tv_sec) * 1.0e3 + (end_time.tv_nsec - start_time.tv_nsec) * 1.0e-6;

    printf("Epoll N = %d Number of FD = %d Time Taken = %.2f ms\n", N, num_fd, time_diff);


    free(buffer);
    for(int i = 0; i < num_fd; i++) 
        close(fds[i]);

    close(epoll_fd);
}


void time_select(int N, int num_fd, int sockfd) {
    
    pid_t child_pid;

    for(int i = 0; i < N; i++) {
        child_pid = fork();
        if(child_pid == 0) {
            send_data(N, num_fd);
            _exit(EXIT_SUCCESS);
        }
        else if(child_pid < 0) {
            err_exit("Error in forking the server process. Exiting....");
        }
    }

    fd_set rset;
    FD_ZERO(&rset);

    num_fd = max(num_fd / N, 1) * N;

    int fds[num_fd], max_fd = 0;
    struct sockaddr_in client;
    memset(&client, 0, sizeof(client));

    void* buffer = malloc(MAX_BUFF);

    socklen_t client_len = sizeof(client);

    for(int i = 0; i < num_fd; i++) {
        fds[i] = accept(sockfd,(struct sockaddr*) &client, &client_len);
         if(fds[i] == -1) 
            err_exit("Error in accept in server. Exiting....\n");
        else if(fds[i] > max_fd)
            max_fd = fds[i];
    }

    int num_of_recv = 0;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);

    int read_fds[num_fd];
    memset(read_fds, 0, sizeof(read_fds));

    for(; num_of_recv < num_fd; ){

        FD_ZERO(&rset);
        for (int i = 0; i < num_fd; i++) {
            if(read_fds[i] == 0)
                FD_SET(fds[i], &rset);
        }

        select(max_fd + 1, &rset, NULL, NULL, NULL);
        
        for(int i = 0; i < num_fd; i++) {
            if (FD_ISSET(fds[i], &rset)){
                memset(buffer, 0, MAX_BUFF);
                if(recv(fds[i], buffer, MAX_BUFF, 0) == -1)
                    err_exit("Error in reading in server. Exiting....\n");
                read_fds[i] = 1;
                num_of_recv++;
            }
        }	
    }

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
    double time_diff = (end_time.tv_sec - start_time.tv_sec) * 1.0e3 + (end_time.tv_nsec - start_time.tv_nsec) * 1.0e-6;

    printf("Select N = %d Number of FD = %d Time Taken = %.2f ms\n", N, num_fd, time_diff);

    FD_ZERO(&rset);
    free(buffer);
    for(int i = 0; i < num_fd; i++) 
        close(fds[i]);
}

int main(int argc, char* argv[]) {
    if(argc < 2)
        err_exit("N number of processes not provided! Exiting....\n");
    
    int N = atoi(argv[1]);

    struct sockaddr_in server;
    int sockfd;
        
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(sockfd == -1) 
        err_exit("Error in socket. Exiting...\n");

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
        
    if(bind(sockfd,(struct sockaddr*) &server, sizeof(server)) == -1) 
        err_exit("Error in bind in server. Exiting....\n");
    
    if(listen (sockfd, 10) == -1)
        err_exit("Error in listen in server. Exiting....\n");

    time_select(N, 10, sockfd);
    time_epoll(N, 10, sockfd);
    time_select(N, 100, sockfd);
    time_epoll(N, 100, sockfd);
    time_select(N, 1000, sockfd);
    time_epoll(N, 1000, sockfd);
}