#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "operations.h"
#include "producer-consumer.h"
#include "logging.h"


#define BUFFER_PRT_SIZE (sizeof(uint8_t) + (sizeof(char) * 288))
#define BUFFER_MSG_SIZE (sizeof(uint8_t) + sizeof(char) * 1024)
#define MAX_BOX_COUNT (64)

typedef struct { 
    uint8_t n_subs;
    uint8_t pub_flag;
    uint8_t box_size;
    char box_name[32];
    char message[1024];
} Box;

/*
typedef struct {
    char box_name[32];
    char pipe_name[256];
} Subscriber;
*/
//Subscriber *sub_array;
Box box_array[MAX_BOX_COUNT];

//> temp flags
int pub_flag = 0;
int manager_flag = 0;
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

size_t get_size_of_msg(char *buffer) { 
    size_t size = 0;

    for (int i = 0; i < strlen(buffer); i++) { 
        if (buffer[i] == '\0')
            break;
        //printf("Char at [%d] -> %c\n", i, buffer[i]);
        size++;
    }

    return size;
}

int find_box(char *box_name) {
    for(int i = 0; i < MAX_BOX_COUNT; i++)
        if(strcmp(box_array[i].box_name, box_name) == 0) 
            return i;
    return -1;
}

//. guarda o tx do subscriber e vai rodando

void pub_connection(char *pipe, char *box) { 
    int rx = open(pipe, O_RDONLY);
    int idx = tfs_open(box, TFS_O_APPEND);
    if (idx == -1) { 
        fprintf(stderr, "[ERR]: box does not exist.\n");
        close(rx);
        return;
    }
    //off_t offset = 0;
    char buffer[BUFFER_MSG_SIZE];
    while (true) {
        
        memset(buffer, '\0', BUFFER_MSG_SIZE);
        ssize_t ret = read(rx, buffer, BUFFER_MSG_SIZE - 1);
        
        if (buffer[0] == 9) {
            size_t size = get_size_of_msg(buffer + sizeof(uint8_t));
            char msg[size];
            memcpy(msg, buffer + sizeof(uint8_t), size);
            msg[size - 1] = '\n';
            //fprintf(stderr, "[INFO]: received %zd B\n", ret);
            //buffer[ret] = 0;
            if (msg[0] != '\0'){
                printf("Written -> %ld\n", tfs_write(idx, msg, size));
                //printf("Message sent = %s\n", msg);
            }
            memset(msg, '\0', sizeof(msg));
        }        
        if (ret == 0) {
            // ret == 0 indicates EOF
            fprintf(stderr, "[INFO]: pipe closed\n");
            tfs_close(idx);
            break;
        } else if (ret == -1) {
            // ret == -1 indicates error
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    box_array[idx].pub_flag = 0;
    tfs_close(idx);
    close(rx);
}

/*
void add_sub_array(char *pipe char *box_name) { 
    for (int i = 0; i < sizeof(sub_array); i++) { 
        if (sub_array->pipe_name[0] != '\0') { 
            strcpy(sub_array[i].pipe_name, pipe);
            strcpy(sub_array[i].box_name, box_name);
        }
    }
}
*/

void sub_connection(char *pipe, char *box) {
    int tx = open(pipe, O_WRONLY);
    
    int idx = tfs_open(box, TFS_O_APPEND);
    if (idx == -1) { 
        fprintf(stderr, "[ERR]: box does not exist.\n");
        return;
    }
    tfs_close(idx);
    idx = tfs_open(box, TFS_O_CREAT);
    char reader[1024];
    void *buffer = malloc(BUFFER_MSG_SIZE);
    void *ptr = buffer + sizeof(uint8_t);

    int i = find_box(box);
    box_array[i].n_subs++;
    
    ssize_t read = tfs_read(idx, reader, sizeof(reader) - 1);
    if (read > 0) {

        memset(buffer, '\0', BUFFER_MSG_SIZE);
        memset(buffer, (uint8_t)10, sizeof(uint8_t));
        memcpy(ptr, reader, sizeof(reader) - 1);
        ssize_t ret = write(tx, buffer, BUFFER_MSG_SIZE);
        if (ret == -1 && errno == EPIPE) { 
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
        }
        else if (ret == 0) {
            // ret == 0 indicates EOF
            fprintf(stderr, "[INFO]: pipe closed\n");
            i = find_box(box);
            box_array[i].n_subs--;
            tfs_close(idx);
        }
    }
    //add_sub_array(pipe, box_array[i].box_name);
    
    tfs_close(idx);
    free(buffer);
    close(tx);
}

void create_box(char *pipe, char *box) {
    size_t size = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(char) * BUFFER_MSG_SIZE - 1;
    char *buffer = malloc(size);
    
    memset(buffer, '\0', size);
    *((uint8_t*) buffer) = 4;
    
    int idx = tfs_open(box, TFS_O_CREAT);
    
    if (idx == -1) {
        *((int32_t*) &buffer[1]) = -1;
        snprintf(&buffer[2], 1024, "%s", "[ERR]: box creation failed");
    } else {
        *((int32_t*) &buffer[1]) = 0;
    
        Box newBox;
        strcpy(newBox.box_name, box);
        newBox.pub_flag = 0;
        newBox.box_size = 0;
        newBox.n_subs = 0;
        for(int i = 0; i < MAX_BOX_COUNT; i++) {
            if(box_array[i].box_name[0] == '\0') {
                box_array[i] = newBox;
                break;
            } else if(i == MAX_BOX_COUNT-1) {
                fprintf(stderr, "[ERR]: no more box space: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
    }

    int tx = open(pipe, O_WRONLY);
  
    ssize_t ret = write(tx, buffer, size);
    if (ret < 0) { 
        fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    tfs_close(idx);
    close(tx);
}

void remove_box(char *pipe, char *box) {

    size_t size = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(char) * BUFFER_MSG_SIZE - 1;  
    char *buffer = malloc(size);

    memset(buffer, '\0', size);
    *((uint8_t*) buffer) = 6;


    int res = tfs_unlink(box);
    if (res == -1) {
        *((int32_t*) &buffer[1]) = -1;
        snprintf(&buffer[2], 1024, "%s", "[ERR]: box removal failed");
    } else {
        *((int32_t*) &buffer[1]) = 0;
        
        int i = find_box(box);
        memset(box_array[i].box_name, 0, 32);
        box_array[i].pub_flag = 0;
    }

    int tx = open(pipe, O_WRONLY);
  
    ssize_t ret = write(tx, buffer, size);
    if (ret < 0) { 
        fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    close(tx);
}

void list_answer_msg(int idx, Box box, int tx, int flag_empty) {
    size_t size;
    uint8_t last = 0;
    if(flag_empty) {
        last = 1;
        size = sizeof(uint8_t)*2 + sizeof(char)*32 + sizeof(char)*5;
    }
    else size = sizeof(uint8_t)*2 + sizeof(char)*32 + sizeof(uint64_t)*3 + sizeof(char)*5;
    char *buffer = malloc(size);

    if((idx == MAX_BOX_COUNT-1 || box_array[idx+1].box_name[0] == '\0') && !flag_empty) last = 1;

    memset(buffer, '\0', size);
    *((uint8_t*) buffer) = 8;
    *((uint8_t*) &buffer[1]) = last;
    snprintf(&buffer[2], 32, "%s", box.box_name);
    
    if(!flag_empty) {
        *((uint8_t*) &buffer[34]) = box.box_size;
        *((uint8_t*) &buffer[42]) = box.pub_flag;
        *((uint8_t*) &buffer[50]) = box.n_subs;
    }
 
    //printf("Buffer -> %s, should be 8%d%s%d%d%d\n", buffer, last, box.box_name, box.box_size, box.pub_flag, box.n_subs);
    //printf("Buffer -> %s\n", buffer);
    
    ssize_t ret = write(tx, buffer, size);
    if(ret < 0) fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));

    free(buffer);
}

void list_box(char *pipe) { 
    int tx = open(pipe, O_WRONLY);

    if (box_array[0].box_name[0] == '\0') {
        list_answer_msg(0, box_array[0], tx, 1);
        return;  
    }
    for(int i = 0; i < MAX_BOX_COUNT; i++) {
        if(box_array[i].box_name[0] == '\0') break;
        //printf("Name of the box -> %s\n", box_array[i].box_name);
        list_answer_msg(i, box_array[i], tx, 0);
    }
    close(tx);
} 

void process_buffer_data(char *buffer) {
    //printf("Buffer -> %d\n", buffer[0]);
    char pipe_name[256];
    char box_name[33];
    box_name [0] = '/';
    memcpy(&pipe_name, buffer+sizeof(uint8_t), sizeof(char) * 256);
    memcpy(box_name + sizeof(char), buffer+sizeof(uint8_t)+(sizeof(char) * 256), sizeof(char) * 32);


    switch ((uint8_t)buffer[0])
    {
    case 1: // register publisher
        int idx = find_box(box_name);
        if (idx != -1 && box_array[idx].pub_flag == 0) {
            box_array[idx].pub_flag = 1; 
            //printf("Buffer -> %s\n", pipe_name);
            //printf("Buffer -> %s\n", box_name);
            printf("Publisher process logged: %s, %s\n", pipe_name, box_name);
            pub_connection(pipe_name, box_name);
        }
        else printf("There's already a publisher associated to this box");
        break;
    case 2: // register subscriber
        printf("Subscriber process logged: %s, %s\n", pipe_name, box_name);
        sub_connection(pipe_name, box_name);
        /* code */
        break;
    case 3: // create box
        create_box(pipe_name, box_name);
        break;
    case 5: // remove box
        remove_box(pipe_name, box_name);
        break;
    case 7: // list boxes
        list_box(pipe_name);
        /* code */
        break;
    default:
        break;
    }


}

void mbroker(char * register_name, size_t size) {
    
    //sub_array = malloc(sizeof(Subscriber) * size);

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
    
    pc_queue_t * queue = malloc(sizeof(pc_queue_t));
    pcq_create(queue, size);

    // loop to receive protocols and execute them
    // pipe stays open 
    int rx = open(register_name,  O_RDONLY);
    int tx = open(register_name, O_WRONLY);

    if (tx == -1 || rx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        pcq_destroy(queue);
        exit(EXIT_FAILURE);
    }


    while (true) {  // condition to maintain server open
        char buffer[BUFFER_PRT_SIZE];
        ssize_t ret = read(rx, buffer, BUFFER_PRT_SIZE - 1);
        if (ret == 0) {
            // ret == 0 indicates EOF
            fprintf(stderr, "[INFO]: pipe closed\n");
            break;
        } else if (ret == -1) {
            // ret == -1 indicates error
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            pcq_destroy(queue);
            exit(EXIT_FAILURE);
        }
        //printf("Buffer-> %s", buffer);
        fprintf(stderr, "[INFO]: received %zd B, code %d\n", ret, buffer[0]);
        buffer[ret] = 0;

        process_buffer_data(buffer);

        //fputs(buffer, stdout);
        //offset += BUFFER_PRT_SIZE - 1;
    }

    pcq_destroy(queue);
    close(tx);
    close(rx);

}

int main(int argc, char **argv) {
    
    if (tfs_init(NULL) != 0)
        printf("[Error]: Unable to init TFS.\n");

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
