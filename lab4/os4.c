/* Efstathia Statha el16190
	Nick Ipsilantis el16086 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <time.h>

#define DEFAULT "\033[30;1m" // used for messages to user
#define RED "\033[31;1m"    // used as father's color for send
#define YELLOW "\033[33m"   // used as child's color for receive
#define MAGENTA "\033[35m" // used as father's color for receive
#define CYAN "\033[36m"    // used as child's color for send
#define WHITE "\033[37m"
#define GREEN "\033[32m" // used for the father to let us know which child is going to be terminated
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define STREQUAL(x, y) ( strncmp((x), (y), strlen(y) ) == 0 )

void usage(const char *prog) {
    printf(DEFAULT"Usage: %s [--host HOST] [--port PORT] [--debug]"WHITE"\n", prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    int status, sd, check, chosen_PORT = 8080;
    char *chosen_HOST = "tcp.akolaitis.os.grnetcloud.net";
    struct sockaddr_in s_in, server;
    struct hostent *hp;
    bool debug_mode = false; // boolean variable to check if we are in debug mode
    bool get_flag; // the flag for sending get to host
    bool ack_flag; // the flag for acknowledgment
    bool per_flag; // the flag for permission
    struct tm * timeinfo;
    int motion, temprature_temp;
    double temperature;
    int light_level;
    char *temp;
    const char s[2] = " ";

    /* checks the arguments*/

    if (argc > 4)
        usage(argv[0]); // we need at least two arguments given to create a child
    else {
        for (int i = 1; i < argc; i++){
            if (STREQUAL(argv[i], "--host")) {
                if (i == argc-1)
                    usage(argv[0]);
                else
                    chosen_HOST = argv[i+1];
            }
            if (STREQUAL(argv[i], "--port")) {
                if (i == argc-1)
                    usage(argv[0]);
                else
                    chosen_PORT = atoi(argv[i+1]);
            }
            else if (STREQUAL(argv[i], "--debug"))
                debug_mode = true;
        }
    }

    sd = socket(AF_INET, SOCK_STREAM, 0); // we open a socket with internet domain and stream communication
    if (sd < 0){
        perror("socket");
        exit(-1);
    }

    s_in.sin_family = AF_INET;
    s_in.sin_port = htons(0);
    s_in.sin_addr.s_addr = htonl(INADDR_ANY);
    check = bind(sd, (struct sockaddr *) &s_in, sizeof(s_in));    // we bind the socket with a port
    if (check < 0){
        perror("bind");
        close(sd);
        exit(-1);
    }

    /* try to connect with server*/

    printf(CYAN"Connecting!"WHITE"\n");

    if (!(hp = gethostbyname(chosen_HOST))) {  //epistrefei 0 sthn periptwsh pou xalaei
        perror("fail");
        exit(-1);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(chosen_PORT);
    bcopy(hp->h_addr,&server.sin_addr.s_addr,hp->h_length);

    check = connect(sd, (struct sockaddr *) &server, sizeof(server));
    if (check < 0){
        perror("connect");
        close(sd);
        exit(-1);
    }

    printf(GREEN"Connected to %s:%d!"WHITE"\n", chosen_HOST,chosen_PORT);

     while(1){
        fd_set inset;
        int maxfd = STDIN_FILENO;

        FD_ZERO(&inset);                // we must initialize before each call to select
        FD_SET(STDIN_FILENO, &inset);   // select will check for input from stdin
        FD_SET(sd, &inset);   // select will check for input from server

        maxfd = MAX(STDIN_FILENO, sd) + 1;

        int ready_fds = select(maxfd, &inset, NULL, NULL, NULL);
        if (ready_fds <= 0) {
            perror("select");
            close(sd);
            continue;   // just try again
        }

        // user has typed something, we can read from stdin without blocking
        if (FD_ISSET(STDIN_FILENO, &inset)) {
            int n_read;
            char buffer[101];

            n_read = read(STDIN_FILENO, buffer, 100);
            if (n_read == -1){
                perror("read");
                close(sd);
                exit(-1);
            }
            buffer[n_read] = '\0';

            if (n_read == 1 && buffer[0] == '\n'){
                continue;
            }
            else if (n_read > 1 && buffer[n_read-1] == '\n') {
                buffer[n_read-1] = '\0';
            }

            if (n_read >= 4 && STREQUAL(buffer, "exit"))
                break;
            else if (n_read >= 4 && STREQUAL(buffer, "help"))
                printf(DEFAULT"Available commands:\n* 'help': Print this help message\n* 'exit': Exit\n* 'get': Retrieve sensor data\n* 'N name surname reason': Ask permission to go out"WHITE"\n");
            else if (n_read >= 3 && STREQUAL(buffer, "get")){
                if(debug_mode)
                    printf(YELLOW"[DEBUG] sent 'get'"WHITE"\n");
                if (write(sd, "get", 4) < -1){
                    perror("write");
                    close(sd);
                    exit(-1);
                }
                get_flag = true; // we will receive get data
                per_flag = false;
            }
            else{
                if(debug_mode)
                    printf(YELLOW"[DEBUG] sent '%s' "WHITE"\n", buffer);
                if (write(sd, buffer, n_read) <-1){
                    perror("write");
                    close(sd);
                    exit(-1);
                }

                if(!ack_flag){
                    per_flag = true; // we will receive permission data
                    get_flag = false;
                }
                else{
                    per_flag = false;
                    get_flag = false;
                }

            }
            bzero(buffer, sizeof(buffer));
        }
        // we have a message from server
        else if (FD_ISSET(sd, &inset)) {
            char buffer[101];
            int n_read = read(sd, buffer, 100);

            if (n_read == -1){
                perror("read");
                exit(-1);
            }

            if(buffer[n_read-1] == '\n')
                buffer[n_read-1] = '\0';
            else
                buffer[n_read] == '\0';

            if(debug_mode)
                printf(YELLOW"[DEBUG] read '%s'"WHITE"\n", buffer);

            if (n_read >= 9 && STREQUAL(buffer, "try again")){
                printf(CYAN"Server response: %s"WHITE"\n", buffer);
                per_flag = false;
            }

            if(get_flag){
                int counter = 1;
                temp = strtok(buffer, s); // we take the first number
                motion = atoi(temp); // now we have the integer motion

                temp = strtok(NULL, s);
                light_level = atoi(temp);

                temp = strtok(NULL, s);
                temprature_temp = atoi(temp);
                temperature = (double)temprature_temp/100;

                temp = strtok(NULL, s);
                time_t lcl = atoi(temp);
                time(&lcl);
                timeinfo = localtime(&lcl);
                printf("------------------------------\n");
                printf(CYAN"Latest event: motion (%d)\nTemperature is: %.2lf\nLight level is: %d\nTimestamp is: %s"WHITE"\n", motion, temperature, light_level, asctime(timeinfo));

            }

            else if(per_flag){
                printf(CYAN"Send verification code: %s"WHITE"\n", buffer);
                ack_flag = true;
            }

            else if(ack_flag){
                printf(CYAN"Response: %s"WHITE"\n", buffer);
                ack_flag = false;
            }

            bzero(buffer, sizeof(buffer));

        }
    }

    close(sd); // close the socket

    return 0;
}
