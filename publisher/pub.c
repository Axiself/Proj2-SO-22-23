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

int pub_pipe(char *pipe_name) { 
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

int msg_sender(char *pipe_name) { 
    int tx = open(pipe_name, O_WRONLY);
    printf("Opening pipe to write\n");
    
    void *buf = malloc(sizeof(uint8_t) + sizeof(char)*1024);
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buf, 0, sizeof(uint8_t) + sizeof(char)*1024);
        memset(buffer, 0, BUFFER_SIZE);
        printf("\n> ");
        char *str = fgets(buffer, BUFFER_SIZE, stdin);
        char *ptr = buf + sizeof(uint8_t);
        memset(buf, 9, sizeof(uint8_t));
        memcpy(ptr, str, sizeof(char)*1024);
        ssize_t ret = write(tx, buf, BUFFER_SIZE);
        if (ret < 0) { 
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    free(buf);
    close(tx);
}

void pub_protocol(char *rpn, char *cpn, char *box) { 
    int tx = open(rpn, O_WRONLY);
    if (tx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } 
    
    size_t size = sizeof(uint8_t) + (sizeof(char) * 256) + (sizeof(char) * 32) - 1;
    void * buffer = malloc(size); // uint8_t + char[256] + char[32]*/
    void * ptr = buffer + sizeof(uint8_t);
    void * ptr2 = buffer + sizeof(uint8_t) + (sizeof(char) * 256);

    uint8_t code = 1;

    memset(buffer, '\0', size);
    memset(buffer, code, sizeof(uint8_t));    
    memcpy(ptr, cpn, strlen(cpn));
    memcpy(ptr2, box, strlen(box));

    ssize_t ret = write(tx, buffer, size);
    if (ret < 0) {
        fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("written-> %ld\n", ret);
    free(buffer);
    close(tx);
}

void publisher(char *register_name, char *pipe_name, char *box_name) { 
    
    if (pub_pipe(pipe_name) != 0) exit(EXIT_FAILURE);

    pub_protocol(register_name, pipe_name, box_name);
    
    msg_sender(pipe_name);

}

int main(int argc, char **argv) {
    
    if (argc != 4) { 
        printf("[Error]: sub <register_pipe_name> <pipe_name> <box_name> \n");
        return -1;
    }

    publisher(argv[1], argv[2], argv[3]);
    //fprintf(stderr, "usage: pub <register_pipe_name> <box_name>\n");
    return -1;
}
