#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "shared_memory.h"

#define NUM_PROCESSES 4
#define MAX_MSG_SIZE 256
#define ERROR_RESULT -1

SharedData *shared_mem_ptr = NULL;

/* This is a usable shared memory implementation
1. Use init_shared_memory to create a shared memory (Note: Must define shared memory parameters with name and size)
(can use default sizeof(SharedDate) or define own size)
2. Use attach_shared_memory for processes to access same memory
3. Use detach_shared_memory for processes if the process has done its job and no longer needs to access the shared memory
4. Use clean_shared_memory to delete the memory created (Note: Must define shared memory parameters with the memory pointer, name, and size)
(default memory pointer is the global variable define above)

(Note for users: This is not yet implemented with synchronization)
*/

// Initialize shared memory (for parent process)
int init_shared_memory(const char *name, size_t size) {
    int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    
    if (shm_fd == -1) {
        perror("Failed to create shared memory!!\n");
        return ERROR_RESULT;
    }
    
    if (ftruncate(shm_fd, size) == -1) {
        perror("Failed to set size for shared memory!!\n");
        close(shm_fd);
        return ERROR_RESULT;
    }
    
    shared_mem_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    if (shared_mem_ptr == MAP_FAILED) {
        perror("Failed to map to the shared memory!!\n");
        close(shm_fd);
        return ERROR_RESULT;
    }
    
    for (int i = 0; i < NUM_PROCESSES; i++) {
        shared_mem_ptr->counter[i] = 0; //Initialise counter for each processes
    }
    
    memset(shared_mem_ptr->buffer, 0, sizeof(shared_mem_ptr->buffer)); //Clear existing buffer
    
    close(shm_fd); //Close fd after successfully created a shared memory
    printf("Successfully created %s with %zu bytes!\n", name, size);
    return 0;
}

// Attach to shared memory (for child processes)
SharedData* attach_shared_memory(const char *name, size_t size) {
    int shm_fd = shm_open(name, O_RDWR, 0666);
    
    if (shm_fd == -1) {
        perror("Failed to attach to memory!!\n");
        return NULL;
    }
    
    SharedData *memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    if (memory == MAP_FAILED) {
        perror("Failed to map to memory!!\n");
        close(shm_fd);
        return NULL;
    }
    
    close(shm_fd); //Close fd after successfully attach to the shared memory
    return memory;
}

// Detach from shared memory
void detach_shared_memory(SharedData *memory, size_t size) {
    if (memory == NULL) {
        perror("Cannot detach from memory (NULL pointer)!!\n");
        return;
    }
    
    if (munmap(memory, size) == -1) {
        perror("Failed to detach from memory!!\n");
        return;
    }
    
    printf("Successfully detached from memory!\n");
}

// Clean up shared memory (unmap and delete)
void clean_shared_memory(SharedData *memory, const char *name, size_t size) {
    if (memory != NULL) {
        if (munmap(memory, size) == -1) {
            printf("Failed to detach from memory for cleaning!!\n");
        }
    }
    
    if (shm_unlink(name) == -1) {
        perror("Failed to delete shared memory!!\n");
    } else {
        printf("Successfully cleaned up %s memory!\n", name);
    }
}