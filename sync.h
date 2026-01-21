#ifndef SYNC_H
#define SYNC_H

#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>

/**
 * Synchronization Module Header
 * 
 * Provides process-shared synchronization primitives for coordinating
 * between parent threads and forked child processes in the Monopoly game.
 * 
 * All synchronization primitives are initialized with PTHREAD_PROCESS_SHARED
 * to enable safe coordination across process boundaries.
 */

/* ============================================================================
 * MUTEX OPERATIONS
 * ============================================================================ */

/**
 * Initialize a process-shared mutex
 * 
 * @param mutex Pointer to pthread_mutex_t to initialize
 * @return 0 on success, -1 on failure
 */
int sync_mutex_init(pthread_mutex_t *mutex);

/**
 * Lock a mutex (blocking)
 * 
 * @param mutex Pointer to pthread_mutex_t to lock
 * @return 0 on success, -1 on failure
 */
int sync_mutex_lock(pthread_mutex_t *mutex);

/**
 * Unlock a mutex
 * 
 * @param mutex Pointer to pthread_mutex_t to unlock
 * @return 0 on success, -1 on failure
 */
int sync_mutex_unlock(pthread_mutex_t *mutex);

/**
 * Try to lock a mutex (non-blocking)
 * 
 * @param mutex Pointer to pthread_mutex_t to try lock
 * @return 0 if locked, 1 if already locked, -1 on failure
 */
int sync_mutex_trylock(pthread_mutex_t *mutex);

/**
 * Destroy a mutex
 * 
 * @param mutex Pointer to pthread_mutex_t to destroy
 * @return 0 on success, -1 on failure
 */
int sync_mutex_destroy(pthread_mutex_t *mutex);

/* ============================================================================
 * SEMAPHORE OPERATIONS
 * ============================================================================ */

/**
 * Initialize a process-shared semaphore with initial value
 * 
 * @param sem Pointer to sem_t to initialize
 * @param initial_value Initial value for the semaphore
 * @return 0 on success, -1 on failure
 */
int sync_sem_init(sem_t *sem, unsigned int initial_value);

/**
 * Wait on a semaphore (blocking, decrement)
 * 
 * @param sem Pointer to sem_t to wait on
 * @return 0 on success, -1 on failure
 */
int sync_sem_wait(sem_t *sem);

/**
 * Try to wait on a semaphore (non-blocking)
 * 
 * @param sem Pointer to sem_t to try wait on
 * @return 0 if wait succeeded, 1 if would block, -1 on failure
 */
int sync_sem_trywait(sem_t *sem);

/**
 * Signal a semaphore (increment)
 * 
 * @param sem Pointer to sem_t to signal
 * @return 0 on success, -1 on failure
 */
int sync_sem_post(sem_t *sem);

/**
 * Get current value of a semaphore
 * 
 * @param sem Pointer to sem_t
 * @param value Pointer to store current value
 * @return 0 on success, -1 on failure
 */
int sync_sem_getvalue(sem_t *sem, int *value);

/**
 * Destroy a semaphore
 * 
 * @param sem Pointer to sem_t to destroy
 * @return 0 on success, -1 on failure
 */
int sync_sem_destroy(sem_t *sem);

/* ============================================================================
 * CONDITION VARIABLE OPERATIONS (optional for future use)
 * ============================================================================ */

/**
 * Initialize a process-shared condition variable
 * 
 * @param cond Pointer to pthread_cond_t to initialize
 * @return 0 on success, -1 on failure
 */
int sync_cond_init(pthread_cond_t *cond);

/**
 * Wait on a condition variable (releases mutex while waiting)
 * 
 * @param cond Pointer to pthread_cond_t
 * @param mutex Pointer to associated pthread_mutex_t
 * @return 0 on success, -1 on failure
 */
int sync_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

/**
 * Signal one waiting thread on a condition variable
 * 
 * @param cond Pointer to pthread_cond_t
 * @return 0 on success, -1 on failure
 */
int sync_cond_signal(pthread_cond_t *cond);

/**
 * Broadcast signal to all waiting threads on a condition variable
 * 
 * @param cond Pointer to pthread_cond_t
 * @return 0 on success, -1 on failure
 */
int sync_cond_broadcast(pthread_cond_t *cond);

/**
 * Destroy a condition variable
 * 
 * @param cond Pointer to pthread_cond_t to destroy
 * @return 0 on success, -1 on failure
 */
int sync_cond_destroy(pthread_cond_t *cond);

#endif // SYNC_H
