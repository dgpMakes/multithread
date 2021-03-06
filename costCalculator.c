
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <pthread.h>
#include "queue.h"
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>


#define NUM_CONSUMERS 1
#define ENTRY_DELIMITER " "
#define MAX_ENTRY_LENGHT 256

//Function declaration
void* producer(void* producer_args);
void* consumer(void* consumer_args);

struct producer_args {
    int start;
    int end;
    char (*lines)[MAX_ENTRY_LENGHT];
    queue* q;
};

struct consumer_args {
    int line_count;
    int total_sum;
    queue* q;
};

/**
 * Entry point
 * @param argc
 * @param argv
 * @return
 */
int main (int argc, const char * argv[] ) { //command, file_name, num_producers, buff_size

    /* Check that arguments are in valid format */

    if(argc != 4){
        fprintf(stderr, "Invalid number of arguments.\n");
        exit(EXIT_FAILURE);
    }

    if(atoi(argv[2])<1 || atoi(argv[3])<1){
        fprintf(stderr, "Invalid parameters type/size.\n");
        exit(EXIT_FAILURE);
    }
   
    /* Check that the file exists and
      the number of producers do not exceed the number of entries in the file. */

    FILE* openedFile = fopen(argv[1], "r");

    if(openedFile == NULL){
        fprintf(stderr, "File does not exist.\n");
        exit(EXIT_FAILURE);
    } 

    char firstLine[MAX_ENTRY_LENGHT];
    int line_count;

    fgets(firstLine, MAX_ENTRY_LENGHT, openedFile);

    if(atoi(firstLine) == 0){
        fprintf(stderr, "Invalid total number of lines.\n");
        exit(EXIT_FAILURE);
    }

    line_count = atoi(firstLine);

    if(line_count < atoi(argv[2])){
        fprintf(stderr, "There can not be more producers than entries.\n");
        exit(EXIT_FAILURE);
    }

    /* Load file in memory line per line */
    char lines[line_count][MAX_ENTRY_LENGHT];
    char readLine[MAX_ENTRY_LENGHT];

    for(int i = 0; i < line_count; i++)
    {
        char* checkError = fgets(readLine, MAX_ENTRY_LENGHT, openedFile);
        if(checkError == NULL && feof(openedFile)){ 
            fprintf(stderr, "Unexpected EOF.\n");
            exit(EXIT_FAILURE);
        }
        
        strncpy(lines[i], readLine, MAX_ENTRY_LENGHT);
    }

    fclose(openedFile);

    /* Distribution of lines per producer */
    
    int num_producers = atoi(argv[2]);
    int circular_queue_size = atoi(argv[3]);
    
    //The queue all the threads will use
    queue* main_queue = queue_init(circular_queue_size);

    //Create producers threads
    pthread_t threads[num_producers];
    struct producer_args p_args [num_producers];

    for(int i = 0; i < num_producers - 1; i++){
        p_args[i].start = i*(line_count/num_producers);
        p_args[i].end =  p_args[i].start + (line_count/num_producers) - 1;
        p_args[i].lines = lines;
        p_args[i].q = main_queue;
        pthread_create(&threads[i], NULL, producer, &p_args[i]);

    }
    //Last producer threads takes the leftovers
    p_args[num_producers - 1].start = (num_producers - 1)*(line_count/num_producers);
    p_args[num_producers - 1].end =  line_count - 1;
    p_args[num_producers - 1].lines = lines;
    p_args[num_producers - 1].q = main_queue;
    pthread_create(&threads[num_producers - 1], NULL, producer, &p_args[num_producers - 1]);

    //Create the consumer
    struct consumer_args c_args;
    c_args.total_sum = 0;
    c_args.q = main_queue;
    c_args.line_count = line_count;
    pthread_t consumer_thread;
    pthread_create(&consumer_thread, NULL, consumer, (void*) &c_args);

    // Join all the threads
    for(int i = 0; i < num_producers; i++){
        pthread_join(threads[i], NULL);
    }

    pthread_join(consumer_thread, NULL);

    printf("Total: %i €.\n", c_args.total_sum);

    queue_destroy(main_queue);
    
    exit(EXIT_SUCCESS);

}


void* producer(void* args){
    struct producer_args* p_args = args;
    
    for(int i = 0; i < p_args->end - p_args->start + 1; i++){
        char line[MAX_ENTRY_LENGHT];
        //Needed for strtok to be thread-safe
        char * context = line;

        strncpy(line, p_args->lines[i + p_args->start], MAX_ENTRY_LENGHT);
        // printf("%s\n", line);
        //Check that indexes are consistent
        int index = atoi(strtok_r(line, ENTRY_DELIMITER, &context));
        if(i + p_args->start + 1 != index) 
            fprintf(stderr, "Warning: unexpected index mismatch.\n");

        struct element elem;
        elem.type = atoi(strtok_r(NULL, ENTRY_DELIMITER, &context));
        elem.time = atoi(strtok_r(NULL, ENTRY_DELIMITER, &context));

        queue_put(p_args->q, elem);
    }

    pthread_exit(NULL);

    return NULL;

}

void* consumer(void* args){

    struct consumer_args* c_args = (struct consumer_args*) args;

    for(int i = 0; i < c_args->line_count; i++){
        struct element result = queue_get(c_args->q);
        

        switch (result.type) {
        case 1:
            c_args->total_sum += result.time;
            break;
        case 2:
            c_args->total_sum += 3*result.time;
            break;
        case 3:
            c_args->total_sum += 10*result.time;
            break;
        default:
            printf("The machine is not defined.%i%i\n", result.type, result.time);
            break;
        }

    }

    pthread_exit(NULL);

    return NULL;

}