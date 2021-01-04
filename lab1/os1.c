/* Efstathia Statha el16190
	Nick Ipsilantis el16086 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>

#define FD_STDIN 0
#define FD_STDOUT 1
#define BUFFER_SIZE 64

#define PERMS 0644

#define STREQUAL(x, y) ( strncmp((x), (y), strlen(y) ) == 0 )

void usage(const char *prog) {
    printf("Usage: %s --input FILENAME, --key NUMBER (in [0,25]) range\n", prog);
    exit(EXIT_FAILURE);
}

typedef enum {
    ENCRYPT,
    DECRYPT
} encrypt_mode;

char caesar(unsigned char ch, encrypt_mode mode, int key)
{
    if (ch >= 'a' && ch <= 'z') {
        if (mode == ENCRYPT) {
            ch += key;
            if (ch > 'z') ch -= 26;
        } else {
            ch -= key;
            if (ch < 'a') ch += 26;
        }
        return ch;
    }

    if (ch >= 'A' && ch <= 'Z') {
        if (mode == ENCRYPT) {
            ch += key;
            if (ch > 'Z') ch -= 26;
        } else {
            ch -= key;
            if (ch < 'A') ch += 26;
        }
        return ch;
    }

    return ch;
}

int main(int argc, char** argv) {
    int n_read, n_write;
    char buffer[BUFFER_SIZE];
    pid_t pid;
    int status, key = -1, counter, flag = 0; // int file argument
    char* input_file = "no name";
    char ch, no_input_message[25] = "Give us the message: \n";

    for (int i = 1; i < argc; i++) {
        if (STREQUAL(argv[i], "--key")) {
            if (i == argc-1) {
                // "--key" was given as the last argument so we let the user know the correct form of arguments
                usage(argv[0]);
            } else {
                // argv[i] == "--key", so argv[i+1] has the given key
                key = atoi(argv[i+1]); // we typecast it to an integer
                if (key > 25 || key < 0)
                    usage(argv[0]);
            }
        }
        else if(STREQUAL(argv[i], "--input")) {
            if (i == argc-1) {
                // "--input" was given as the last argument so we need to read the name of the file through std input
                flag = 1;
            } else {
                // argv[i] == "--input", so argv[i+1] has the given input file name

                input_file = argv[i+1];
            }
        }
    }

    if (key == -1)
        usage(argv[0]); // if the user didnt gave us a key we let him know of the correct form

    pid = fork(); // here we generate the first child

    if (pid == -1)
        perror("fork");

    else if (pid == 0){

        //first child's code for encrypting

        int fd_in, fd_out;
        if (!STREQUAL(input_file, "no name"))
            fd_in = open(input_file, O_RDONLY);
        else{
            fd_in = FD_STDIN; // if the name of the file wasn't given as argument then we are gonna take the message from the std input
            write(FD_STDOUT, no_input_message, 25); // message for the user to give the message
        }

        if( access("encrypted.txt", F_OK ) != -1 ) {
            if (remove("encrypted.txt") == 0)
                fd_out = open("encrypted.txt", O_CREAT|O_WRONLY, PERMS); // if the file "encrypted already exists we destroy it and recreate it"
            }
        else {
            fd_out = open("encrypted.txt", O_CREAT|O_WRONLY, PERMS); // else we just create it
        }

        if (fd_in == -1) {
            perror("open");
            exit(-1);
        }

        do {
            n_read = read(fd_in, buffer, BUFFER_SIZE);
            if (n_read == -1) {
                perror("read");
                exit(-1);
            }

            for(int i = 0; i < n_read; i++){
                buffer[i] = caesar(buffer[i], ENCRYPT, key);
            }

            n_write = write(fd_out, buffer, n_read);
            if (n_write < n_read) {
                perror("write");
                exit(-1);
            }
        } while (n_read > 0);

        close(fd_in);
        close(fd_out);

        exit(0);
    }

    else{
        wait(&status); // father waits his child to die
    }

    pid = fork(); // here we generate the second child

    if (pid == -1)
        perror("fork");
    else if (pid == 0){
        // second child's code for decrypting

        int fd_in = open("encrypted.txt", O_RDONLY);
        if (fd_in == -1) {
            perror("open");
            exit(-1);
        }

        do {
            n_read = read(fd_in, buffer, BUFFER_SIZE);
            if (n_read == -1) {
                perror("read");
                exit(-1);
            }

            for(int i = 0; i < n_read; i++){
                buffer[i] = caesar(buffer[i], DECRYPT, key);
            }

            n_write = write(FD_STDOUT, buffer, n_read);
            if (n_write < n_read) {
                perror("write");
                exit(-1);
            }
        } while (n_read > 0);

        close(fd_in);

        exit(0);
    }
    else{
        wait(&status); // father waits for his child to die
    }

    return 0;
}
