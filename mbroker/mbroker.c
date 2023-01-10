#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "producer-consumer.h"
#include "logging.h"

/*
int create_mbroker(char * register_pipe_name, size_t max_sessions) {
    pc_queue_t * queue = malloc(sizeof(pc_queue_t));
    pcq_create(queue, max_sessions);
    
    mbroker m = { 
        register_pipe_name: register_pipe_name, 
        max_sessions: max_sessions, 
        pcq_queue: queue
    };

    // Remove pipe if it does not exist
    if (unlink(m.register_pipe_name) != 0 && errno != ENOENT) { 
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Create pipe
    if (mkfifo(m.register_pipe_name, 0640) != 0) { 
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } 



    return m;
}
*/
void send_msg(int tx, char const *str) {
    size_t len = strlen(str);
    size_t written = 0;

    while (written < len) {
        ssize_t ret = write(tx, str + written, len - written);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        written += (size_t) ret;
    }
}
/*
void writeToPipe(mbroker d) {
    // Open pipe for writing
    // This waits for someone to open it for reading
    int tx = open(d.register_pipe_name, O_WRONLY);
    if (tx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // The parent likes classic rock:
    // https://www.youtube.com/watch?v=Kc71KZG87X4
    send_msg(tx, "SKIDIBOP BOP BOP\n");
    send_msg(tx, "YES YES YES YES\n");
    send_msg(tx, "Biiibarabruuuuuuu\n");

    fprintf(stderr, "[INFO]: closing pipe\n");
    close(tx);
}*/

int mbroker(char * register_name, size_t size) {
    
    // Remove pipe if it does not exist
    if (unlink(register_name) != 0 && errno != ENOENT) { 
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Create pipe
    if (mkfifo(register_name, 0640) != 0) { 
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } 

    // loop to receive protocols and execute them
    // pipe stays open 
    int tx = open(register_name,  O_RDONLY);
    int rx = open(register_name, O_WRONLY);

    while (true) {
        
    }

}

int main(int argc, char **argv) {
    int leave = 0;
    char str1[10];
    printf("Number of args-> %d\n", argc);
    if (argc != 3) {
        printf("[Error]: mbroker <register_pipe_name> <max_sessions> \n");
        return -1;
    }

    printf("programe name: %s\n", argv[0]);
    printf("argv[1]-> %s;\nargv[2]-> %s;\n", argv[1], argv[2]);
    //fprintf(stderr, "usage: mbroker <register_pipe_name> <max_sessions>\n");
    mbroker(argv[1], (size_t) strtol(argv[2], NULL, 10));


    return -1;
}
