#include "sync.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

/*
 All primitives are initialized with PTHREAD_PROCESS_SHARED attribute
 to safely work across process boundaries (fork).
 */


int sync_mutex_init(pthread_mutex_t *mutex) {
    if (mutex == NULL) {
        fprintf(stderr, "[SYNC] Error: mutex_init received NULL pointer\n");
        return -1;
    }

    pthread_mutexattr_t attr;
    
    // Initialize mutex attributes
    if (pthread_mutexattr_init(&attr) != 0) {
        fprintf(stderr, "[SYNC] Error: failed to init mutex attributes\n");
        return -1;
    }

    // Set as process-shared (allows fork'd children to use it)
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
        fprintf(stderr, "[SYNC] Error: failed to set pshared attribute\n");
        pthread_mutexattr_destroy(&attr);
        return -1;
    }

    // Initialize the mutex
    if (pthread_mutex_init(mutex, &attr) != 0) {
        fprintf(stderr, "[SYNC] Error: failed to init mutex: %s\n", strerror(errno));
        pthread_mutexattr_destroy(&attr);
        return -1;
    }

    pthread_mutexattr_destroy(&attr);
    return 0;
}

int sync_mutex_lock(pthread_mutex_t *mutex) {
    if (mutex == NULL) {
        fprintf(stderr, "[SYNC] Error: mutex_lock received NULL pointer\n");
        return -1;
    }

    int result = pthread_mutex_lock(mutex);
    if (result != 0) {
        fprintf(stderr, "[SYNC] Error: failed to lock mutex: %s\n", strerror(result));
        return -1;
    }

    return 0;
}

int sync_mutex_unlock(pthread_mutex_t *mutex) {
    if (mutex == NULL) {
        fprintf(stderr, "[SYNC] Error: mutex_unlock received NULL pointer\n");
        return -1;
    }

    int result = pthread_mutex_unlock(mutex);
    if (result != 0) {
        fprintf(stderr, "[SYNC] Error: failed to unlock mutex: %s\n", strerror(result));
        return -1;
    }

    return 0;
}

int sync_mutex_trylock(pthread_mutex_t *mutex) {
    if (mutex == NULL) {
        fprintf(stderr, "[SYNC] Error: mutex_trylock received NULL pointer\n");
        return -1;
    }

    int result = pthread_mutex_trylock(mutex);
    
    if (result == 0) {
        return 0;  // Successfully locked
    } else if (result == EBUSY) {
        return 1;  // Already locked by another thread
    } else {
        fprintf(stderr, "[SYNC] Error: trylock failed: %s\n", strerror(result));
        return -1;
    }
}

int sync_mutex_destroy(pthread_mutex_t *mutex) {
    if (mutex == NULL) {
        fprintf(stderr, "[SYNC] Error: mutex_destroy received NULL pointer\n");
        return -1;
    }

    int result = pthread_mutex_destroy(mutex);
    if (result != 0) {
        fprintf(stderr, "[SYNC] Error: failed to destroy mutex: %s\n", strerror(result));
        return -1;
    }

    return 0;
}

//SEMAPHORE OPERATIONS

int sync_sem_init(sem_t *sem, unsigned int initial_value) {
    if (sem == NULL) {
        fprintf(stderr, "[SYNC] Error: sem_init received NULL pointer\n");
        return -1;
    }

    // Initialize semaphore with pshared=1 (process-shared)
    if (sem_init(sem, 1, initial_value) == -1) {
        fprintf(stderr, "[SYNC] Error: failed to init semaphore: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int sync_sem_wait(sem_t *sem) {
    if (sem == NULL) {
        fprintf(stderr, "[SYNC] Error: sem_wait received NULL pointer\n");
        return -1;
    }

    while (sem_wait(sem) == -1) {
        if (errno == EINTR) {
            // Interrupted by signal, retry
            continue;
        }
        fprintf(stderr, "[SYNC] Error: failed to wait on semaphore: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int sync_sem_trywait(sem_t *sem) {
    if (sem == NULL) {
        fprintf(stderr, "[SYNC] Error: sem_trywait received NULL pointer\n");
        return -1;
    }

    int result = sem_trywait(sem);
    
    if (result == 0) {
        return 0;  // Successfully decremented
    } else if (errno == EAGAIN) {
        return 1;  // Semaphore value would go below 0
    } else {
        fprintf(stderr, "[SYNC] Error: trywait failed: %s\n", strerror(errno));
        return -1;
    }
}

int sync_sem_post(sem_t *sem) {
    if (sem == NULL) {
        fprintf(stderr, "[SYNC] Error: sem_post received NULL pointer\n");
        return -1;
    }

    if (sem_post(sem) == -1) {
        fprintf(stderr, "[SYNC] Error: failed to post semaphore: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int sync_sem_getvalue(sem_t *sem, int *value) {
    if (sem == NULL || value == NULL) {
        fprintf(stderr, "[SYNC] Error: sem_getvalue received NULL pointer\n");
        return -1;
    }

    if (sem_getvalue(sem, value) == -1) {
        fprintf(stderr, "[SYNC] Error: failed to get semaphore value: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int sync_sem_destroy(sem_t *sem) {
    if (sem == NULL) {
        fprintf(stderr, "[SYNC] Error: sem_destroy received NULL pointer\n");
        return -1;
    }

    if (sem_destroy(sem) == -1) {
        fprintf(stderr, "[SYNC] Error: failed to destroy semaphore: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/* ============================================================================
 * CONDITION VARIABLE OPERATIONS
 * ============================================================================ */

int sync_cond_init(pthread_cond_t *cond) {
    if (cond == NULL) {
        fprintf(stderr, "[SYNC] Error: cond_init received NULL pointer\n");
        return -1;
    }

    pthread_condattr_t attr;

    // Initialize condition variable attributes
    if (pthread_condattr_init(&attr) != 0) {
        fprintf(stderr, "[SYNC] Error: failed to init cond attributes\n");
        return -1;
    }

    // Set as process-shared
    if (pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
        fprintf(stderr, "[SYNC] Error: failed to set pshared attribute on cond\n");
        pthread_condattr_destroy(&attr);
        return -1;
    }

    // Initialize the condition variable
    if (pthread_cond_init(cond, &attr) != 0) {
        fprintf(stderr, "[SYNC] Error: failed to init condition variable: %s\n", strerror(errno));
        pthread_condattr_destroy(&attr);
        return -1;
    }

    pthread_condattr_destroy(&attr);
    return 0;
}

int sync_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    if (cond == NULL || mutex == NULL) {
        fprintf(stderr, "[SYNC] Error: cond_wait received NULL pointer\n");
        return -1;
    }

    int result = pthread_cond_wait(cond, mutex);
    if (result != 0) {
        fprintf(stderr, "[SYNC] Error: failed to wait on condition variable: %s\n", strerror(result));
        return -1;
    }

    return 0;
}

int sync_cond_signal(pthread_cond_t *cond) {
    if (cond == NULL) {
        fprintf(stderr, "[SYNC] Error: cond_signal received NULL pointer\n");
        return -1;
    }

    int result = pthread_cond_signal(cond);
    if (result != 0) {
        fprintf(stderr, "[SYNC] Error: failed to signal condition variable: %s\n", strerror(result));
        return -1;
    }

    return 0;
}

int sync_cond_broadcast(pthread_cond_t *cond) {
    if (cond == NULL) {
        fprintf(stderr, "[SYNC] Error: cond_broadcast received NULL pointer\n");
        return -1;
    }

    int result = pthread_cond_broadcast(cond);
    if (result != 0) {
        fprintf(stderr, "[SYNC] Error: failed to broadcast condition variable: %s\n", strerror(result));
        return -1;
    }

    return 0;
}

int sync_cond_destroy(pthread_cond_t *cond) {
    if (cond == NULL) {
        fprintf(stderr, "[SYNC] Error: cond_destroy received NULL pointer\n");
        return -1;
    }

    int result = pthread_cond_destroy(cond);
    if (result != 0) {
        fprintf(stderr, "[SYNC] Error: failed to destroy condition variable: %s\n", strerror(result));
        return -1;
    }

    return 0;
}
