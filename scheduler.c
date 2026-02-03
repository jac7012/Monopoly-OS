#include "scheduler.h"
#include "sync.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

/**
 * Scheduler Module Implementation
 * 
 * Implements Round Robin scheduling with turn management for Monopoly game.
 * The scheduler is designed to:
 * - Run as a dedicated thread in the parent process
 * - Manage turn transitions between players
 * - Coordinate via shared memory with fork'd child processes
 * - Skip disconnected players automatically
 * - Ensure only one player acts at a time using synchronization
 */

// Global shared memory pointers
static SchedulerState *scheduler_state = NULL;
static const char *SCHEDULER_SHM_NAME = "/monopoly_scheduler";

/**
 * Find the next active and connected player
 * Skips disconnected players in round-robin way
 * 
 * @return Index of next active player, or -1 if none found
 */
static int find_next_active_player(void) {
    if (scheduler_state == NULL || scheduler_state->num_players == 0) {
        return -1;
    }

    int start_idx = scheduler_state->current_player_idx;
    int idx = (start_idx + 1) % scheduler_state->num_players;
    int attempts = 0;

    // Try to find an active player, maximum num_players attempts
    while (attempts < scheduler_state->num_players) {
        if (scheduler_state->players[idx].is_connected && 
            scheduler_state->players[idx].is_active) {
            return idx;
        }
        idx = (idx + 1) % scheduler_state->num_players;
        attempts++;
    }

    return -1;  // No active players found
}

/**
 * Update turn signals for all players
 * Only the current player's signal should be non-zero
 */
static void update_turn_signals(void) {
    if (scheduler_state == NULL) {
        return;
    }

    for (int i = 0; i < scheduler_state->num_players; i++) {
        if (i == scheduler_state->current_player_idx) {
            // Set signal for current player (but only if not already non-zero)
            int current_val;
            if (sync_sem_getvalue(&scheduler_state->turn_signal[i], &current_val) == 0) {
                if (current_val == 0) {
                    sync_sem_post(&scheduler_state->turn_signal[i]);
                }
            }
        } else {
            // Clear signals for other players
            int val;
            if (sync_sem_getvalue(&scheduler_state->turn_signal[i], &val) == 0) {
                // Drain the semaphore if it's positive
                while (val > 0) {
                    if (sync_sem_trywait(&scheduler_state->turn_signal[i]) == 0) {
                        sync_sem_getvalue(&scheduler_state->turn_signal[i], &val);
                    } else {
                        break;
                    }
                }
            }
        }
    }
}

/* ============================================================================
 * PUBLIC SCHEDULER FUNCTIONS
 * ============================================================================ */

int scheduler_init(int num_players) {
    // Validate player count
    if (num_players < MIN_PLAYERS || num_players > MAX_PLAYERS) {
        fprintf(stderr, "[SCHEDULER] Error: num_players must be %d-%d, got %d\n",
                MIN_PLAYERS, MAX_PLAYERS, num_players);
        return -1;
    }

    // Create or open shared memory
    int shm_fd = shm_open(SCHEDULER_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        fprintf(stderr, "[SCHEDULER] Error: shm_open failed: %s\n", strerror(errno));
        return -1;
    }

    // Set size of shared memory
    size_t size = sizeof(SchedulerState);
    if (ftruncate(shm_fd, size) == -1) {
        fprintf(stderr, "[SCHEDULER] Error: ftruncate failed: %s\n", strerror(errno));
        close(shm_fd);
        return -1;
    }

    // Map shared memory
    scheduler_state = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (scheduler_state == MAP_FAILED) {
        fprintf(stderr, "[SCHEDULER] Error: mmap failed: %s\n", strerror(errno));
        close(shm_fd);
        return -1;
    }

    close(shm_fd);

    // Initialize scheduler state
    memset(scheduler_state, 0, sizeof(SchedulerState));
    scheduler_state->num_players = num_players;
    scheduler_state->active_player_count = 0;
    scheduler_state->current_player_idx = 0;
    scheduler_state->round_number = 0;
    scheduler_state->total_moves = 0;
    scheduler_state->game_in_progress = false;
    scheduler_state->scheduler_running = false;

    if (logger_init("game.log") == -1) {
        fprintf(stderr, "[SCHEDULER] Warning: logger failed to initialize\n");
    } else {
        logger_log("Scheduler initialized with %d players", num_players);
    }

    // Initialize players
    for (int i = 0; i < num_players; i++) {
        scheduler_state->players[i].player_id = i;
        scheduler_state->players[i].is_connected = false;
        scheduler_state->players[i].is_active = false;
        scheduler_state->players[i].turn_count = 0;
        scheduler_state->players[i].client_thread = 0;

        // Initialize per-player turn signal (start at 0)
        if (sync_sem_init(&scheduler_state->turn_signal[i], 0) == -1) {
            fprintf(stderr, "[SCHEDULER] Error: failed to init turn signal for player %d\n", i);
            munmap(scheduler_state, size);
            shm_unlink(SCHEDULER_SHM_NAME);
            return -1;
        }
    }

    // Initialize synchronization primitives
    if (sync_mutex_init(&scheduler_state->scheduler_lock) == -1) {
        fprintf(stderr, "[SCHEDULER] Error: failed to init scheduler_lock\n");
        munmap(scheduler_state, size);
        shm_unlink(SCHEDULER_SHM_NAME);
        return -1;
    }

    if (sync_cond_init(&scheduler_state->turn_changed) == -1) {
        fprintf(stderr, "[SCHEDULER] Error: failed to init turn_changed condition\n");
        munmap(scheduler_state, size);
        shm_unlink(SCHEDULER_SHM_NAME);
        return -1;
    }

    printf("[SCHEDULER] Initialized with %d players\n", num_players);
    return 0;
}

pthread_t scheduler_start(void) {
    if (scheduler_state == NULL) {
        fprintf(stderr, "[SCHEDULER] Error: scheduler_init must be called first\n");
        return 0;
    }

    pthread_t tid;
    int result = pthread_create(&tid, NULL, scheduler_thread_main, NULL);
    
    if (result != 0) {
        fprintf(stderr, "[SCHEDULER] Error: pthread_create failed: %s\n", strerror(result));
        return 0;
    }

    printf("[SCHEDULER] Scheduler thread started (TID: %lu)\n", tid);
    return tid;
}

int scheduler_player_connect(int player_id) {
    if (scheduler_state == NULL) {
        fprintf(stderr, "[SCHEDULER] Error: scheduler not initialized\n");
        return -1;
    }

    if (player_id < 0 || player_id >= scheduler_state->num_players) {
        fprintf(stderr, "[SCHEDULER] Error: invalid player_id %d\n", player_id);
        return -1;
    }

    if (sync_mutex_lock(&scheduler_state->scheduler_lock) == -1) {
        return -1;
    }

    if (scheduler_state->players[player_id].is_connected) {
        fprintf(stderr, "[SCHEDULER] Warning: player %d already connected\n", player_id);
        sync_mutex_unlock(&scheduler_state->scheduler_lock);
        return 0;  // Not an error, already connected
    }

    scheduler_state->players[player_id].is_connected = true;
    scheduler_state->players[player_id].is_active = true;
    scheduler_state->active_player_count++;

    printf("[SCHEDULER] Player %d connected (active: %d)\n", 
           player_id, scheduler_state->active_player_count);
    logger_log("Player %d connected (active=%d)", player_id, scheduler_state->active_player_count);

    sync_cond_broadcast(&scheduler_state->turn_changed);
    sync_mutex_unlock(&scheduler_state->scheduler_lock);
    return 0;
}

int scheduler_player_disconnect(int player_id) {
    if (scheduler_state == NULL) {
        fprintf(stderr, "[SCHEDULER] Error: scheduler not initialized\n");
        return -1;
    }

    if (player_id < 0 || player_id >= scheduler_state->num_players) {
        fprintf(stderr, "[SCHEDULER] Error: invalid player_id %d\n", player_id);
        return -1;
    }

    if (sync_mutex_lock(&scheduler_state->scheduler_lock) == -1) {
        return -1;
    }

    if (!scheduler_state->players[player_id].is_connected) {
        sync_mutex_unlock(&scheduler_state->scheduler_lock);
        return 0;  // Already disconnected
    }

    scheduler_state->players[player_id].is_connected = false;
    scheduler_state->players[player_id].is_active = false;
    scheduler_state->active_player_count--;

    printf("[SCHEDULER] Player %d disconnected (active: %d)\n", 
           player_id, scheduler_state->active_player_count);
    logger_log("Player %d disconnected (active=%d)", player_id, scheduler_state->active_player_count);

    // If current player just disconnected, advance turn
    if (scheduler_state->current_player_idx == player_id) {
        int next_idx = find_next_active_player();
        if (next_idx >= 0) {
            scheduler_state->current_player_idx = next_idx;
            update_turn_signals();
            sync_cond_broadcast(&scheduler_state->turn_changed);
        }
    }

    sync_mutex_unlock(&scheduler_state->scheduler_lock);
    return 0;
}

int scheduler_get_current_player(void) {
    if (scheduler_state == NULL) {
        return -1;
    }

    // No lock needed for reading a single int (atomic on most systems)
    return scheduler_state->current_player_idx;
}

int scheduler_is_my_turn(int player_id) {
    if (scheduler_state == NULL) {
        return -1;
    }

    if (player_id < 0 || player_id >= scheduler_state->num_players) {
        return -1;
    }

    // Check if it's my turn
    if (scheduler_state->current_player_idx == player_id && 
        scheduler_state->players[player_id].is_connected) {
        return 1;
    }
    return 0;
}

int scheduler_get_num_players(void) {
    if (scheduler_state == NULL) {
        return -1;
    }
    return scheduler_state->num_players;
}

int scheduler_get_round(void) {
    if (scheduler_state == NULL) {
        return -1;
    }
    return scheduler_state->round_number;
}

long scheduler_get_total_moves(void) {
    if (scheduler_state == NULL) {
        return -1;
    }
    return scheduler_state->total_moves;
}

int scheduler_advance_turn(void) {
    if (scheduler_state == NULL) {
        fprintf(stderr, "[SCHEDULER] Error: scheduler not initialized\n");
        return -1;
    }

    if (sync_mutex_lock(&scheduler_state->scheduler_lock) == -1) {
        return -1;
    }

    // Find next active player
    int next_idx = find_next_active_player();
    if (next_idx < 0) {
        fprintf(stderr, "[SCHEDULER] Error: no active players available\n");
        sync_mutex_unlock(&scheduler_state->scheduler_lock);
        return -1;
    }

    // Update current player
    scheduler_state->current_player_idx = next_idx;
    scheduler_state->total_moves++;

    // Update turn signals
    update_turn_signals();

    // Signal turn change
    sync_cond_broadcast(&scheduler_state->turn_changed);

    printf("[SCHEDULER] Advance turn to player %d (total moves: %ld)\n",
           next_idx, scheduler_state->total_moves);
    logger_log("Turn changed to player %d (total_moves=%ld)", next_idx, scheduler_state->total_moves);

    int result = next_idx;
    sync_mutex_unlock(&scheduler_state->scheduler_lock);
    return result;
}

int scheduler_end_game(void) {
    if (scheduler_state == NULL) {
        return -1;
    }

    if (sync_mutex_lock(&scheduler_state->scheduler_lock) == -1) {
        return -1;
    }

    scheduler_state->game_in_progress = false;
    sync_cond_broadcast(&scheduler_state->turn_changed);

    printf("[SCHEDULER] Game end signal sent\n");
    logger_log("Game ended");

    sync_mutex_unlock(&scheduler_state->scheduler_lock);
    return 0;
}

int scheduler_stop(pthread_t scheduler_tid) {
    if (scheduler_tid == 0) {
        fprintf(stderr, "[SCHEDULER] Error: invalid thread ID\n");
        return -1;
    }

    // Wait for thread to finish
    int result = pthread_join(scheduler_tid, NULL);
    if (result != 0) {
        fprintf(stderr, "[SCHEDULER] Error: pthread_join failed: %s\n", strerror(result));
        return -1;
    }

    printf("[SCHEDULER] Scheduler thread stopped\n");
    return 0;
}

int scheduler_cleanup(void) {
    if (scheduler_state == NULL) {
        return 0;  // Already cleaned up
    }

    // Destroy synchronization primitives
    for (int i = 0; i < scheduler_state->num_players; i++) {
        sync_sem_destroy(&scheduler_state->turn_signal[i]);
    }

    sync_mutex_destroy(&scheduler_state->scheduler_lock);
    sync_cond_destroy(&scheduler_state->turn_changed);

    // Unmap shared memory
    size_t size = sizeof(SchedulerState);
    if (munmap(scheduler_state, size) == -1) {
        fprintf(stderr, "[SCHEDULER] Error: munmap failed: %s\n", strerror(errno));
        return -1;
    }

    // Remove shared memory object
    if (shm_unlink(SCHEDULER_SHM_NAME) == -1) {
        fprintf(stderr, "[SCHEDULER] Error: shm_unlink failed: %s\n", strerror(errno));
        return -1;
    }

    scheduler_state = NULL;
    printf("[SCHEDULER] Cleanup complete\n");
    logger_shutdown();
    return 0;
}

/* ============================================================================
 * SCHEDULER THREAD MAIN LOOP
 * ============================================================================ */

void *scheduler_thread_main(void *arg) {
    (void)arg;  // Unused parameter

    if (scheduler_state == NULL) {
        fprintf(stderr, "[SCHEDULER-THREAD] Error: scheduler not initialized\n");
        return NULL;
    }

    printf("[SCHEDULER-THREAD] Scheduler thread running\n");

    if (sync_mutex_lock(&scheduler_state->scheduler_lock) == -1) {
        return NULL;
    }

    scheduler_state->scheduler_running = true;
    // DO NOT set game_in_progress to true - wait for server to start game

    sync_mutex_unlock(&scheduler_state->scheduler_lock);

    // Main scheduler loop
    while (true) {
        if (sync_mutex_lock(&scheduler_state->scheduler_lock) == -1) {
            break;
        }

        // Check if game should continue
        if (!scheduler_state->game_in_progress) {
            sync_mutex_unlock(&scheduler_state->scheduler_lock);
            usleep(500000);  // Sleep 500ms while waiting for game to start
            continue;  // Keep checking
        }

        // Wait a bit before advancing (allows game logic time to process moves)
        sync_mutex_unlock(&scheduler_state->scheduler_lock);
        usleep(100000);  // 100ms

        if (sync_mutex_lock(&scheduler_state->scheduler_lock) == -1) {
            break;
        }

        // Advance turn ONLY if game is actually in progress
        if (scheduler_state->game_in_progress && scheduler_state->active_player_count > 0) {
            int next_idx = find_next_active_player();
            if (next_idx >= 0) {
                scheduler_state->current_player_idx = next_idx;
                scheduler_state->total_moves++;

                // Check if we completed a full round
                if (next_idx == 0) {
                    scheduler_state->round_number++;
                    printf("[SCHEDULER-THREAD] Round %d completed\n", scheduler_state->round_number);
                    logger_log("Round %d completed", scheduler_state->round_number);
                }

                update_turn_signals();
                sync_cond_broadcast(&scheduler_state->turn_changed);

                printf("[SCHEDULER-THREAD] Turn advanced to player %d (round %d, move %ld)\n",
                       next_idx, scheduler_state->round_number, scheduler_state->total_moves);
                logger_log("Turn advanced to player %d (round=%d, move=%ld)",
                           next_idx, scheduler_state->round_number, scheduler_state->total_moves);
            }
        }

        sync_mutex_unlock(&scheduler_state->scheduler_lock);
    }

    if (sync_mutex_lock(&scheduler_state->scheduler_lock) == -1) {
        return NULL;
    }

    scheduler_state->scheduler_running = false;
    sync_mutex_unlock(&scheduler_state->scheduler_lock);

    printf("[SCHEDULER-THREAD] Scheduler thread exiting\n");
    return NULL;
}

/* ============================================================================
 * ACCESSOR FUNCTIONS
 * ============================================================================ */

SchedulerState *scheduler_get_state(void) {
    return scheduler_state;
}
