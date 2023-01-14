#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include "logging.h"

#define BUFFER_SIZE (1024)
#define BUFFER_MSG_SIZE (sizeof(uint8_t) + sizeof(char) * BUFFER_SIZE)

int sub_pipe(char *pipe_name) { 
    // Remove pipe if it does not exist
    if (unlink(pipe_name) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", pipe_name,
                strerror(errno));
        return -1;
    }

    // Create pipe
    if (mkfifo(pipe_name, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void sig_handler(int sig) { 

    static int count = 0;
    // UNSAFE: This handler uses non-async-signal-safe functions (printf(),
    // exit();)
    if (sig == SIGINT) {
        // In some systems, after the handler call the signal gets reverted
        // to SIG_DFL (the default action associated with the signal).
        // So we set the signal handler back to our function after each trap.
        //
        if (signal(SIGINT, sig_handler) == SIG_ERR) {
            exit(EXIT_FAILURE);
        }
        count++;
        fprintf(stderr, "\nCaught SIGINT (%d)\n", count);
        //write(stdout, "Caught SIGINT (%d)\n", count);
        return; // Resume execution at point of interruption
    }

    // Must be SIGQUIT - print a message and terminate the process
    fprintf(stderr, "Caught SIGQUIT - that's all folks!\n");
    //write(stdout, "Caught SIGQUIT - that's all folks!\n", 36);
    return;
}

void msg_receiver(char *pipe_name) {
    int rx = open(pipe_name, O_RDONLY);
    //off_t offset = 0;
    void * buffer = malloc(BUFFER_MSG_SIZE);
    while (true) {
        memset(buffer, 0, BUFFER_MSG_SIZE);
        ssize_t ret = read(rx, buffer, BUFFER_MSG_SIZE);
        if (ret == -1) {
            // ret == -1 indicates error
            free(buffer);
            close(rx);
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            break;
        }
        if (signal(SIGINT, sig_handler) == SIG_ERR) { 
            exit(EXIT_FAILURE);
        }
        printf("Buffer received -> %s", (char*) buffer);
        //fprintf(stderr, "[INFO]: received %zd B\n", ret);
        //buffer[ret] = 0;
        //fputs(buffer, stdout);
        //offset += BUFFER_SIZE - 1;
    }
    free (buffer);
    close(rx);
}

void sub_protocol(char *rpn, char *cpn, char *box) { 
    int tx = open(rpn, O_WRONLY);
    if (tx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } 
    
    size_t size = sizeof(uint8_t) + (sizeof(char) * 256) + (sizeof(char) * 32) - 1;
    void * buffer = malloc(size); // uint8_t + char[256] + char[32]*/
    void * ptr = buffer + sizeof(uint8_t);
    void * ptr2 = buffer + sizeof(uint8_t) + (sizeof(char) * 256);

    uint8_t code = 2;

    memset(buffer, '\0', size);
    memset(buffer, code, sizeof(uint8_t));    
    memcpy(ptr, cpn, strlen(cpn));
    memcpy(ptr2, box, strlen(box));

    ssize_t ret = write(tx, buffer, size);
    if (ret < 0) {
        fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    free(buffer);
    close(tx);
}

void subcriber(char *register_name, char *pipe_name, char *box_name) {

    if (sub_pipe(pipe_name) != 0) exit(EXIT_FAILURE);
    
    sub_protocol(register_name, pipe_name, box_name);
    msg_receiver(pipe_name);

}

int main(int argc, char **argv) {
    if (argc != 4) { 
        printf("[Error]: sub <register_pipe_name> <pipe_name> <box_name> \n");
        return -1;
    }
    //fprintf(stderr, "usage: sub <register_pipe_name> <box_name>\n");
    // order of received params
    subcriber(argv[1], argv[2], argv[3]);


    return -1;
}
