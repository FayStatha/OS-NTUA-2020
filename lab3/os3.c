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
#include <stdbool.h>

#define DEFAULT "\033[30;1m" // used for messages to user
#define RED "\033[31;1m"    // used as father's color for send
#define YELLOW "\033[33m"   // used as child's color for receive
#define MAGENTA "\033[35m" // used as father's color for receive
#define CYAN "\033[36m"    // used as child's color for send
#define WHITE "\033[37m"
#define GREEN "\033[32m" // used for the father to let us know which child is going to be terminated

#define STREQUAL(x, y) ( strncmp((x), (y), strlen(y) ) == 0 )

pid_t *all_pids;
int **father_to_children, **children_to_father;
int total_children, my_index, counter = -1; // total number of children && each process index && counter for child's selection

void usage(const char *prog) {
    printf(DEFAULT"Usage: %s <nChildren> [--random] [--round-robin]"WHITE"\n", prog);
    exit(EXIT_FAILURE);
}

void SigtermChild(){

    close(father_to_children[my_index][0]);
    close(children_to_father[my_index][1]);
    exit(0);
    return;
}

void SigtermFather(){
    int children_cnt = total_children-1;

    // NEED TO CHECK FOR DEAD CHILDREN

    while(children_cnt >= 0){
         if (all_pids[children_cnt] != -1){
            printf(GREEN"[Parent] Will terminate (SIGTERM) Child %d: %d."WHITE"\n", children_cnt, all_pids[children_cnt]);
            if (kill(all_pids[children_cnt], SIGTERM) == -1) perror("kill");
        }
        children_cnt--;
    }

    return;
}

/* function to close all the pipes if there is a problem while trying to open a new one*/
void close_the_pipes(int index, int flag){
    for (int i = 0; i < index; i++){
        for (int j = 0; j < 2; j ++){
            close(father_to_children[i][j]);
            close(children_to_father[i][j]);
        }
    }
    if (flag == 1){
        close(children_to_father[index][0]);
        close(children_to_father[index][1]);
    }
}

/* function for a child to close all the useless pipes*/
void close_childs_pipes(){
    for (int i = 0; i < total_children; i++){
        for (int j = 0; j < 2; j ++){
            if (i != my_index){
                close(father_to_children[i][j]);
                close(children_to_father[i][j]);
            }
        }
    }
    close(father_to_children[my_index][1]);
    close(children_to_father[my_index][0]);
}

bool isNumeric(const char *str){
    int i = 0;
    while(str[i] != '\0')
    {
        if(str[i] < '0' || str[i] > '9')
            return false;
        str++;
    }
    return true;
}

/*function to select which child will receive the message from the father*/
int select_child(int method_flag){
    if (method_flag == 1){
        counter = (counter+1)%total_children;
    }
    else{
        counter = rand();
        counter %= total_children;
    }
    return counter;
}


int main(int argc, char** argv) {
    int status, method_flag = 1; // method_flag = 1 means that we use round-robin and 0 stands for random, starts with one cause round robin is default method
    pid_t childs_pid, dead_child;
    struct sigaction father_term;
    int tries; // we set this variable to give 3 chances in each fork to succed

    if (argc < 2 || argc > 3)
        usage(argv[0]); // we need at least two arguments given to create a child
    else {
        total_children = atoi(argv[1]); // the number of children is the second argument
        if (total_children < 1)
            usage(argv[0]); // if the number given for total children isn't correct
        else if (argc == 3) {
            if (STREQUAL(argv[2], "--random"))
                method_flag = 0;
            else if (STREQUAL(argv[2], "--round-robin"))
                method_flag = 1;
            else
                usage(argv[0]); // if the third argument is wrong
        }
    }

    all_pids = (pid_t *)malloc(total_children * sizeof(pid_t)); // all_pids is the array with all the childrens' pids

    children_to_father = (int **)malloc(total_children * sizeof(int*));
    for(int i = 0; i < total_children; i++)
        children_to_father[i] = (int *)malloc(2 * sizeof(int)); // n*2 array for n pipes to send messages from children to father

    father_to_children = (int **)malloc(total_children * sizeof(int*));
    for(int i = 0; i < total_children; i++)
        father_to_children[i] = (int *)malloc(2 * sizeof(int)); // n*2 array for n pipes to send messages from father to children

    for (int i = 0; i < total_children; i++){
        if (pipe(children_to_father[i]) == -1){
            close_the_pipes(i, 0); // close the pipes that have been created
            perror("pipe");
            exit(-1); // open pipe for the father to send messages if there is an error exit
        }
        if (pipe(father_to_children[i]) == -1){
            close_the_pipes(i, 1); // close the pipes that have been created
            perror("pipe");
            exit(-1); // open pipe fot the father to receive messages if there is an error exit
        }
    }

    father_term.sa_handler = SigtermFather;
    sigaction(SIGTERM, &father_term, NULL); //we set SIGTERM sigaction for father to the right function

    for (int index = 0; index < total_children; index++) {
        tries = 3;
        childs_pid = fork();

        while (childs_pid == -1 && tries >= 0){     // we try to fork a maximum of 3 times
            if (tries == 0){
                perror("fork");
                exit(-1);
            }
            else {
                perror("fork");
                childs_pid = fork();
                tries--;
            }
        }

        if(childs_pid == 0){
            // every child's code

            pid_t my_pid = getpid();       // every child sets my_pid variable to its pid value
            int val;
            my_index = index;

            struct sigaction child_term;
            child_term.sa_handler = SigtermChild;
            sigaction(SIGTERM, &child_term, NULL);

            close_childs_pipes(); // close all the useless pipes

            while(1){
                int n_read;
                n_read = read(father_to_children[my_index][0], &val, sizeof(int));
                if (n_read == -1){
                    perror("read");
                    exit(-1);
                }

                printf(YELLOW"[Child %d] [%d] Child received %d!"WHITE"\n", my_index, my_pid, val);

                val++;

                sleep(5);

                if (write(children_to_father[my_index][1], &val, sizeof(int)) == -1){
                    perror("write");
                    exit(-1);
                }
                printf(CYAN"[Child %d] [%d] Child Finished hard work, writing back %d"WHITE"\n", my_index, my_pid, val);
            }
            //for safety, will this ever execute?!?!
            close(father_to_children[my_index][0]); //close the pipe for reading
            close(children_to_father[my_index][1]); //close the pipe for writing
            exit(0); //child dies
        }

        else{
            //father's code while creating each child
            all_pids[index] = childs_pid; // if the child is created then we need to keep it's pid
        }
    }

    for (int i = 0; i < total_children; i++){
        close(children_to_father[i][1]); // close useless pipes
        close(father_to_children[i][0]); // close useless pipes
    }

    // main(father's) code here

    while(1){
        fd_set inset;
        int maxfd = STDIN_FILENO;

        FD_ZERO(&inset);                // we must initialize before each call to select
        FD_SET(STDIN_FILENO, &inset);   // select will check for input from stdin
        for (int i = 0; i < total_children; i++){
            FD_SET(children_to_father[i][0], &inset);   // select will check for input from pipes
            if (children_to_father[i][0] > maxfd)
                maxfd = children_to_father[i][0];
        }

        int ready_fds = select(maxfd+1, &inset, NULL, NULL, NULL);
        if (ready_fds <= 0) {
            perror("select");
            continue;   // just try again
        }

        // user has typed something, we can read from stdin without blocking
        if (FD_ISSET(STDIN_FILENO, &inset)) {
            int n_read, value, child;
            char buffer[101];

            n_read = read(STDIN_FILENO, buffer, 100);
            if (n_read == -1){
                perror("read");
                exit(-1);
            }
            buffer[n_read] = '\0';

            if (n_read == 1 && buffer[0] == '\n') buffer[0] = 'c'; // put a random value in the buffer so that the program types the message
            else if (n_read > 1 && buffer[n_read-1] == '\n') {
                buffer[n_read-1] = '\0';
            }

            if (n_read >= 4 && STREQUAL(buffer, "exit"))
                break;
            else if (n_read >= 4 && STREQUAL(buffer, "help"))
                printf(DEFAULT"Type a number to send job to a child!"WHITE"\n");
            else if (isNumeric(buffer)){
                value = atoi(buffer);

                child = select_child(method_flag);

                printf(RED"[Parent] Assigned %d to child %d"WHITE"\n", value, child);

                if(write(father_to_children[child][1], &value, sizeof(int)) == -1){
                    perror("write");
                    exit(-1);
                }
            }
            else
                printf(DEFAULT"Type a number to send job to a child!"WHITE"\n");
        }

        for (int i = 0; i < total_children; i++){
            if (FD_ISSET(children_to_father[i][0], &inset)) {
                int val, n_read;

                n_read = read(children_to_father[i][0], &val, sizeof(int));
                if (n_read == -1){
                    perror("read");
                    exit(-1);
                }

                printf(MAGENTA"[Parent] Received result from child %d --> %d"WHITE"\n", i, val);
            }
        }
    }

    raise(SIGTERM); // father is here because of an exit typed into stdin

    for(int i = total_children; i > 0; i--){
        printf(DEFAULT"[Parent] Waiting for %d children that are still working!"WHITE"\n", i);

        dead_child = wait(&status); // Father waits for each child to die and keeps dead childs pid in order to set it to -1

        for (int i = 0; i < total_children; i++){
            if (all_pids[i] == dead_child)
                all_pids[i] = -1;           // dead child has -1 value as pid
        }
    }

    for (int i = 0; i < total_children; i++){
        close(children_to_father[i][0]); // close pipe for reading
        close(father_to_children[i][1]); // close pipe for writing
    }

    printf(DEFAULT"[Parent] Will exit: all children have been terminated."WHITE"\n"); // Father dies after all his children have died
    printf("\n");

    return 0;
}
