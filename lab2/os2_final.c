/* Efstathia Statha el16190
	Nick Ipsilantis el16086 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

pid_t *all_pids;
int cnt, my_index, total_children;
pid_t my_pid;

void usage(const char *prog) {
    printf("Usage: %s Delay1 Delay2 ... DelayN\n", prog);
    exit(EXIT_FAILURE);
}

void childCreationPrint(){
    printf("[Child Process %d: %d] Was created and will pause!\n", my_index, my_pid);
    printf("\n");
    return;
}

void childWakeUpPrint(){
    printf("[Child Process %d: %d] Is starting!\n", my_index, my_pid);
    printf("\n");
    return;
}

void fatherCreationPrint(){
    printf("[Father Process: %d] Was created and will create %d children!\n", my_pid, total_children);
    printf("\n");
    return;
}

void fatherWaitingPrint(int i){
    printf("[Father Process: %d] Waiting for %d children that are still working!!\n", my_pid, i);
    printf("\n");
    return;
}

void SigtermChild(){
    exit(0);
    return;
}

void SigtermFather(){
    int children_cnt = total_children;

    // CHECKING IF CHILD IS DEAD

    while(children_cnt > 0){
         if (all_pids[children_cnt] != -1){
            printf("[Father Process: %d] Will terminate (SIGTERM) child process %d: %d.\n", my_pid, children_cnt, all_pids[children_cnt]);
            printf("\n");
            if (kill(all_pids[children_cnt], SIGTERM) == -1) perror("kill");
        }
        children_cnt--;
    }

    return;
}

void Siguser1Child(){
    printf("[Child Process %d: %d] Value: %d!\n", my_index, my_pid, cnt);
    printf("\n");
    return;
}

void Siguser2Child(){
    printf("[Child Process: %d] Echo!\n", my_pid);
    printf("\n");
    return;
}

void Siguser1Father(){
    int children_cnt = total_children;
    printf("[Father Process: %d] Will ask current values (SIGUSER1) from all active children processes!\n", my_pid);
    printf("\n");

    /* CHECKING IF CHILD IS DEAD */

    while(children_cnt > 0){
        if (all_pids[children_cnt] != -1)
            if (kill(all_pids[children_cnt], SIGUSR1) == -1) perror("kill");
        children_cnt--;
    }
    return;
}

void Siguser2Father(){
    printf("[Process: %d] Echo!\n", my_pid);
    printf("\n");
    return;
}

void SigalarmChild(){
    printf("[Child Process %d: %d] Time Expired! Final Value: %d!\n", my_index, my_pid, cnt);
    printf("\n");
    exit(0);
    return;
}

int main(int argc, char** argv) {
    int status, delay, seconds, max_time = 200;
    pid_t childs_pid, dead_child;
    struct sigaction father_term, father_user1, father_user2;
    int tries; // we set this variable to give 3 chances for each fork to succeed

    total_children = argc-1;

    all_pids = (pid_t *)malloc(argc * sizeof(pid_t)); // all_pids[0] will not have a value

    if (argc < 2) usage(argv[0]); // we need at least two arguments given to create a child

    printf("Maximum execution time of children is set to %d seconds\n", max_time);
    printf("\n");

    my_pid = getpid(); // father sets my_pid variable to his pid
    fatherCreationPrint();

    // we set the sigactions for each signal received by father to the right fucntion

    father_term.sa_handler = SigtermFather;
    father_term.sa_flags = SA_RESTART;          // the call is automatically restarted after the signal handler returns
    sigaction(SIGTERM, &father_term, NULL);

    for ( my_index = 1; my_index < argc; my_index++) {
        tries = 3;
        childs_pid = fork();

        while (childs_pid == -1 && tries >= 0){     // we try to fork for a maximum of 3 times
            if (tries == 0){
                perror("fork");
                while (raise(SIGTERM) != 0);
            }
            else {
                perror("fork");
                childs_pid = fork();
                tries--;
            }
        }

        if(childs_pid == 0){
            // every child's code

            struct sigaction child_term, child_sigalarm, child_siguser1, child_siguser2; // declaring all signal actions and then set the signal action to the right functions

            child_term.sa_handler = SigtermChild;
            sigaction(SIGTERM, &child_term, NULL);

            child_sigalarm.sa_handler = SigalarmChild;
            sigaction(SIGALRM, &child_sigalarm, NULL);

            child_siguser1.sa_handler = Siguser1Child;
            sigaction(SIGUSR1, &child_siguser1, NULL);

            child_siguser2.sa_handler = Siguser2Child;
            sigaction(SIGUSR2, &child_siguser2, NULL);

            delay = atoi(argv[my_index]);
            if (delay <= 0) usage(argv[0]); // if given delay is ok set it as this child's delay
            my_pid = getpid();              // every child sets my_pid variable to its pid value

            childCreationPrint();

            while (raise(SIGSTOP)!= 0); // pause the child

            alarm(max_time); //let the child know that the time has expired, always succesful

            childWakeUpPrint();

            cnt = 0;
            while(1){
                seconds = delay;
                cnt++;
                while ((seconds = sleep(seconds)) != 0); // while the sleep function is interrupted sleep for as long time as left
            }

            exit(0); //child dies
        }
        else{
            //father's code while creating each child

            all_pids[my_index] = childs_pid; // if the child is created then we need to keep it's pid

            while (waitpid(childs_pid, &status, WUNTRACED) == -1); // father waits the newborn child to pause
        }
    }

    sigaction(SIGINT, &father_term, NULL);

    father_user1.sa_handler = Siguser1Father;
    father_user1.sa_flags = SA_RESTART;          // the call is automatically restarted after the signal handler returns
    sigaction(SIGUSR1, &father_user1, NULL);

    father_user2.sa_handler = Siguser2Father;
    father_user2.sa_flags = SA_RESTART;          // the call is automatically restarted after the signal handler returns
    sigaction(SIGUSR2, &father_user2, NULL);

    for(int i = 1; i < argc; i++){
        if (kill(all_pids[i], SIGCONT) == -1) perror("kill"); // Father wakes up every child, if there is an error we let the user know
    }

    for(int i = argc-1; i > 0; i--){
        fatherWaitingPrint(i); // Prints how many children are still running

        dead_child = wait(&status); // Father waits for each of his children to die

        for(int j = 1; j < argc; j++){
            if (all_pids[j] == dead_child) all_pids[j] = -1; // all_pids[i] == -1 means that the child i is dead
        }
    }

    printf("[Father Process: %d] Will exit: all children have been terminated.\n", my_pid); // Father dies after all his children have died;
    printf("\n");

    return 0;
}
