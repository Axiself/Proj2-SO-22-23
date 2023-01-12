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

#define BUFFER_SIZE (128)

int man_pipe(char *pipe_name) { 
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

void answer_handler(char *pipe_name) { 
    int rx = open(pipe_name, O_RDONLY);
    //off_t offset = 0;
    while (true) {
        char buffer[BUFFER_SIZE];
        ssize_t ret = read(rx, buffer, BUFFER_SIZE - 1);
        if (ret == 0) {
            // ret == 0 indicates EOF
            fprintf(stderr, "[INFO]: pipe closed\n");
            break;
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

void man_protocol(uint8_t code, char *rpn, char *cpn, char *box) { 
    int tx = open(rpn, O_WRONLY);
    if (tx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    void *buffer;
    size_t size;
    if (code != 7) {
        size = sizeof(uint8_t) + (sizeof(char) * 256) + (sizeof(char) * 32) - 1;
        buffer = malloc(size); // uint8_t + char[256] + char[32]*/
        void * ptr = buffer + sizeof(uint8_t);
        void * ptr2 = buffer + sizeof(uint8_t) + (sizeof(char) * 256);

        memset(buffer, '\0', size);
        memset(buffer, code, sizeof(uint8_t));    
        memcpy(ptr, cpn, strlen(rpn));
        memcpy(ptr2, box, strlen(box));

    } else {
        size = sizeof(uint8_t) + (sizeof(char) * 256);
        buffer = malloc(size);
        void *ptr = buffer + sizeof(uint8_t);

        memset(buffer, '\0', size);
        memset(buffer, code, sizeof(uint8_t));
        memcpy(ptr, cpn, strlen(cpn));
    }  

    ssize_t ret = write(tx, buffer, size);
    if (ret < 0) {
        fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("written-> %ld", ret);

    close(tx);
}

void manager(char *register_name, char *pipe_name, char *type, char *box_name) {
    
    man_pipe(pipe_name);
    if (strcmp(type, "create") == 0) { 
        man_protocol(3, register_name, pipe_name, box_name);
    } else if (strcmp(type, "remove") == 0) { 
        man_protocol(5, register_name, pipe_name, box_name);
    }

    answer_handler(pipe_name);

}   

void manager_list(char *register_name, char *pipe_name) { 
    man_pipe(pipe_name);
    man_protocol(7, register_name, pipe_name, NULL);
    answer_handler(pipe_name);

}

static void print_usage() {
    fprintf(stderr, "usage: \n"
                    "   manager <register_pipe_name> <pipe_name> create <box_name>\n"
                    "   manager <register_pipe_name> <pipe_name> remove <box_name>\n"
                    "   manager <register_pipe_name> <pipe_name> list\n");
}

int main(int argc, char **argv) {
    if (argc == 5) {
        manager(argv[1], argv[2], argv[3], argv[4]);
    } else if(argc == 4 && strcmp(argv[3], "list") == 0) {
        manager_list(argv[1], argv[2]);
    } else {
        print_usage();
        //printf("[Error]: manager <register_pipe_name> <pipe_name> <type> <box_name> \n");
        exit(EXIT_FAILURE);
    }
    print_usage();
    return -1;
}   