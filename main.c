#include "shared_memory.h"
#include "scheduler.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

void child_process(int process_id) {
    SharedData *mem = attach_shared_memory("/test", sizeof(SharedData)); //Access the created shared memory
    
    if (mem == NULL) {
        exit(1);
    }
    snprintf(mem->buffer[process_id], MAX_MSG_SIZE, "Hello from Process %d (PID=%d)", process_id, getpid()); // write into buffer for each processes
    
    printf("Process %d wrote: %s\n", process_id, mem->buffer[process_id]);

    mem->counter[process_id]++;
    
    sleep(1);
    usleep(process_id * 1000000);
    
    printf("\nProcess %d reading messages:\n", process_id);
    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (i != process_id && strlen(mem->buffer[i]) > 0) { //do not read from own process
            printf("  From Process %d: %s\n", i, mem->buffer[i]);
        }
    }
    
    detach_shared_memory(mem, sizeof(SharedData));
    exit(0);
}

int main() {
    printf("Testing shared memory with all processes\n\n");
    
    if (init_shared_memory("/test", sizeof(SharedData)) == -1) {
        return 1;
    }
    
    for (int i = 0; i < NUM_PROCESSES; i++) {
        pid_t pid = fork();
        
        if (pid == 0) {
            child_process(i); //only run for the child process, parent process will not run unless error
        } else if (pid < 0) {
            printf("Failed to fork process %d\n", i);
        }
    }
    
    for (int i = 0; i < NUM_PROCESSES; i++) {
        wait(NULL); //wait for one child proccess to terminate
    }
    
    printf("\nBuffer contents from all processes\n");
    for (int i = 0; i < NUM_PROCESSES; i++) {
        printf("Process %d: %s (sent %d messages)\n", i, shared_mem_ptr->buffer[i], shared_mem_ptr->counter[i]);
    }
    
    clean_shared_memory(shared_mem_ptr, "/test", sizeof(SharedData));

    printf("\nStarting scheduler/logger demo\n\n");
    if (scheduler_init(4) == 0) {
        pthread_t sched_tid = scheduler_start();
        for (int i = 0; i < 4; i++) {
            scheduler_player_connect(i);
        }

        // Simulate a few turn advances
        for (int i = 0; i < 6; i++) {
            usleep(150000);
            scheduler_advance_turn();
        }

        scheduler_end_game();
        scheduler_stop(sched_tid);
        scheduler_cleanup();
    }
    
    return 0;
}
