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

#include "logging.h"

#define BUFFER_SIZE (1024)

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

int msg_receiver(char *pipe_name) {
    int rx = open(pipe_name, O_RDONLY);
    //off_t offset = 0;
    while (true) {
        char buffer[BUFFER_SIZE];
        ssize_t ret = read(rx, buffer, BUFFER_SIZE - 1);
        if (ret == 0) {
            // ret == 0 indicates EOF
            fprintf(stderr, "[INFO]: pipe closed\n");
            return 0;
        } else if (ret == -1) {
            // ret == -1 indicates error
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "[INFO]: received %zd B\n", ret);
        buffer[ret] = 0;
        fputs(buffer, stdout);
        //offset += BUFFER_SIZE - 1;
    }
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
    memcpy(ptr, cpn, strlen(rpn));
    memcpy(ptr2, box, strlen(box));

    ssize_t ret = write(tx, buffer, size);
    if (ret < 0) {
        fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("written-> %ld", ret);

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
