#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_LEN 120
#define MAX_BUFF_LEN 512

typedef struct file_info {
    off_t size;
    off_t offset;
} FILE_INFO;

void err_exit(const char * err_msg) {
    perror(err_msg);
    _exit(EXIT_FAILURE);
}

void read_data(int sock_fd, int N) {
    char buffer[MAX_BUFF_LEN];

    while (1) {
        FILE_INFO file_info;
        int fd, num_read;

        struct msghdr mssg;
        memset(&mssg, 0, sizeof(mssg));

        struct cmsghdr *cmptr;
        struct iovec iov;
        iov.iov_base = &file_info;
        iov.iov_len = sizeof(file_info);

        mssg.msg_name = NULL;
        mssg.msg_namelen = 0;
        mssg.msg_iov = &iov;
        mssg.msg_iovlen = 1;
        
        cmptr = malloc(CMSG_LEN(sizeof(int)));
        mssg.msg_control = cmptr;
        mssg.msg_controllen = CMSG_LEN(sizeof(int));
        
        if (recvmsg(sock_fd, &mssg, 0) == -1) {
            err_exit("Error in recvmsg in child. Exiting..\n");
        }

        cmptr = CMSG_FIRSTHDR(&mssg);
        
        fd = *((int *) CMSG_DATA(cmptr));
        
        num_read = pread(fd, buffer, file_info.size / N, file_info.offset);
        
        if (buffer[num_read - 1] != '\0')
            buffer[num_read - 1] = '\0';

        printf("\n***PID*** %d\n\n%s\n\n", getpid(), buffer);

        void* buffer = malloc(MAX_LEN);
        
        if (send(sock_fd, buffer, MAX_LEN, 0) == -1)
            perror("Error in send. Exiting...\n");
        
        close(fd);
    }

    close(sock_fd);
}

int main(int argc, char* argv[]) {
    if(argc < 2)
        err_exit("N number of processes not provided! Exiting....\n");
    
    int N = atoi(argv[1]);

    char file_name[MAX_LEN];

    int child_fds[N];

    int fds[2];
    pid_t child_pid;

    for(int i = 0; i < N; i++) {
        if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1)
            err_exit("Error in creating unix socket pair. Exiting....\n");
        
        child_pid = fork();
        if(child_pid == 0) {
            //child
            close(fds[0]);
            read_data(fds[1], N);
            _exit(EXIT_SUCCESS);
        }
        else if(child_pid > 0) {
            //parent
            close(fds[1]);
            child_fds[i] = fds[0];
        }
        else {
            err_exit("Error in fork. Exiting....\n");
        }
    }

    while (1) {
        printf("\n\nEnter Filename: ");
        fflush(stdout);
        fgets(file_name, 100, stdin);
        if (file_name[strlen(file_name) - 1] == '\n')
            file_name[strlen(file_name) - 1] = '\0';

        size_t file_len = strlen(file_name);

        if(file_len == 0)
            continue;

        int fd = open(file_name, O_RDONLY);
        
        if(fd == -1)
            err_exit("Error in opening file. Exiting....\n");

        struct stat file_stat;
            if (stat(file_name, &file_stat) == -1)
                err_exit("Error in stat. Exiting....\n");
        
        FILE_INFO file_info;
        file_info.size = file_stat.st_size;

        struct msghdr mssg;
        memset(&mssg, 0, sizeof(mssg));

        struct cmsghdr *cmptr;
        struct iovec iov;
        iov.iov_base = &file_info;
        iov.iov_len = sizeof(file_info);

        mssg.msg_name = NULL;
        mssg.msg_namelen = 0;
        mssg.msg_iov = &iov;
        mssg.msg_iovlen = 1;
        
        cmptr = malloc(CMSG_LEN(sizeof(int)));
        cmptr -> cmsg_level = SOL_SOCKET;
        cmptr -> cmsg_type = SCM_RIGHTS;
        cmptr -> cmsg_len = CMSG_LEN(sizeof(int));
        mssg.msg_control = cmptr;
        mssg.msg_controllen = CMSG_LEN(sizeof(int));
        *((int *) CMSG_DATA(cmptr)) = fd;
        
        for (int i = 0; i < N; i++) {
            file_info.offset = (i * file_info.size) / N;
            
            if (sendmsg(child_fds[i], &mssg, 0) == -1)
                err_exit("Error in sendmsg. Exiting....\n");

            void* buffer = malloc(MAX_LEN);
            
            if (recv(child_fds[i], buffer, MAX_LEN, 0) == -1)
                err_exit("Error in recv. Exiting....\n");
        }

        close(fd);
    }

    for (int i = 0; i < N; ++i) {
        wait(NULL);
        close(child_fds[i]);
    }


}