#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>

void errorExit(const char *errmsg)
{
    perror(errmsg);
    _exit(EXIT_FAILURE);
}

int count_sigusr1;
int num_valid_process;
int valid_process[12];
int is_sigalrm_rcvd;

void handler_sigusr1(int sig)
{
    ++count_sigusr1;
}

void handler_sigalrm(int sig)
{
    is_sigalrm_rcvd = 1;
}

int main(int argc, char *argv[])
{

    if (argc < 2)
        errorExit("\nArgument 'N' (int) not supplied!\n");

    int N = atoi(argv[1]);
    
    // Debug Print N 
    // printf("\nN=%d\n", N);

    for (int i = 0; i < N; ++i)
    {
        pid_t pid = fork();

        if (pid == 0)
        {
            // One of the N processes
            pid = getpid();
            int z = pid % 13;

            // Creating process debug print
            // printf("Created (%d/%d) of n processes with PID=%d!!\n", i + 1, N, pid);

            int is_process;
            pid_t pid_child;
            for (int ii = 0; ii < z; ++ii)
            {
                pid_child = fork();
                
                is_process = pid_child;

                if (pid_child == 0)
                {
                    pid_child = getpid();
                    // Creating process debug print
                    // printf("Created (%d/%d) of z processes for n=%d with PID=%d!!\n", ii + 1, z, i + 1, pid_child);
                    break;
                }
            }

            if (is_process >= 0)
            {
                pid_child = getpid();
                // One of the z children + parent basically not any error

                num_valid_process = 0;

                // Valid pid identification
                for (int iii = 1; iii <= 12; ++iii)
                {
                    if (kill(pid_child + iii, 0) == 0)
                    {
                        valid_process[num_valid_process++] = pid_child + iii;
                    }
                }

                count_sigusr1 = 0;
                is_sigalrm_rcvd = 0;

                struct sigaction sigusr1, sigalrm;
                sigusr1.sa_handler = handler_sigusr1;
                sigusr1.sa_flags = 0;
                sigemptyset(&sigusr1.sa_mask);

                sigalrm.sa_handler = handler_sigalrm;
                sigalrm.sa_flags = 0;
                sigemptyset(&sigalrm.sa_mask);

                if (sigaction(SIGUSR1, &sigusr1, NULL) == -1)
                    errorExit("\nError in sigaction SIGUSR1!\n");
                if (sigaction(SIGALRM, &sigalrm, NULL) == -1)
                    errorExit("\nError in sigaction SIGALRM!\n");
                sleep(2);

                // Initial generation of SIGUSR1 signals

                // 2 sec sigalrm timer creation
                struct itimerval time_int;
                time_int.it_interval = (struct timeval) {2, 0};
                time_int.it_value = (struct timeval) {2, 0};

                if (setitimer(ITIMER_REAL, &time_int, NULL) == -1)
                    errorExit("\nError in setitimer!\n");

                for (int idx = 0; idx < num_valid_process; ++idx)
                {
                    kill(valid_process[idx], SIGUSR1);
                }

                while (1)
                {
                    // Pause until signal is received
                    pause();
                    if (is_sigalrm_rcvd)
                    {
                        printf("PID=%d with number of SIGUSR1 signals=%d\n", pid_child, count_sigusr1);
                        if (count_sigusr1 == 0)
                        {
                            // Debug print statement
                            printf("Exit process with PID=%d...\n", pid_child);
                            exit(0);
                        }

                        // Reset count for next 2 seconds
                        count_sigusr1 = 0;

                       // Send SIGUSR1 signals
                        for (int idx = 0; idx < num_valid_process; ++idx)
                        {
                            kill(valid_process[idx], SIGUSR1);
                        }

                        is_sigalrm_rcvd = 0;
                    }
                }
            }
            else
                errorExit("\nError in forking one of z children!\n");

            break; // Child process should not create forks
        }
        else if (pid > 0)
        {
            // Main Process which created N processes
            // Do nothing
        }
        else
            errorExit("\nError in forking one of the N processes!\n");
    }

    return 0;
}