#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stddef.h>

#define NUM_PROCESSES 4
#define MAX_MSG_SIZE 256
#define ERROR_RESULT -1

typedef struct {
    int counter[NUM_PROCESSES]; //counter for tracking data sent for each processes
    char buffer[NUM_PROCESSES][MAX_MSG_SIZE]; //buffer for each processes to send data to a Shared Memory
} SharedData;

extern SharedData *shared_mem_ptr;// Global shared memory pointer

// Function declarations ONLY
int init_shared_memory(const char *name, size_t size);
SharedData* attach_shared_memory(const char *name, size_t size);
void detach_shared_memory(SharedData *memory, size_t size);
void clean_shared_memory(SharedData *memory, const char *name, size_t size);

#endif