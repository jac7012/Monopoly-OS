#include "scheduler.h"
#include "shared_memory.h"
#include "sync.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define NUM_TEST_PLAYERS 3
#define NUM_TEST_ROUNDS 2

/**
 * Simulates a player process that:
 * 1. Connects to the scheduler
 * 2. Waits for their turn
 * 3. Writes a message to shared memory
 * 4. Reads messages from other players
 */
void player_process(int player_id, const char *shm_name) {
    printf("[Player %d] Process started (PID: %d)\n", player_id, getpid());
    
    // Attach to shared memory
    SharedData *mem = attach_shared_memory(shm_name, sizeof(SharedData));
    if (mem == NULL) {
        fprintf(stderr, "[Player %d] Failed to attach to shared memory\n", player_id);
        exit(1);
    }
    printf("[Player %d] Attached to shared memory\n", player_id);
    
    // Take turns for a few rounds
    for (int round = 0; round < NUM_TEST_ROUNDS; round++) {
        // Wait for my turn (busy wait - in real game would use semaphore)
        while (scheduler_get_current_player() != player_id) {
            usleep(50000);  // 50ms
            
            // Check if game ended
            if (!scheduler_get_state()->game_in_progress) {
                printf("[Player %d] Game ended, exiting\n", player_id);
                detach_shared_memory(mem, sizeof(SharedData));
                exit(0);
            }
        }
        
        printf("[Player %d] ✓ My turn! (Round %d)\n", player_id, round);
        
        // Write message to shared memory
        snprintf(mem->buffer[player_id], MAX_MSG_SIZE, 
                "Player %d move in round %d (PID=%d)", 
                player_id, round, getpid());
        mem->counter[player_id]++;
        
        printf("[Player %d] Wrote: %s\n", player_id, mem->buffer[player_id]);
        
        // Read messages from other players
        printf("[Player %d] Reading other players' messages:\n", player_id);
        for (int i = 0; i < NUM_TEST_PLAYERS; i++) {
            if (i != player_id && strlen(mem->buffer[i]) > 0) {
                printf("[Player %d]   -> %s\n", player_id, mem->buffer[i]);
            }
        }
        
        // Simulate doing some work
        usleep(200000);  // 200ms
        
        printf("[Player %d] Turn complete\n", player_id);
    }
    
    printf("[Player %d] All rounds complete, disconnecting\n", player_id);
    scheduler_player_disconnect(player_id);
    
    detach_shared_memory(mem, sizeof(SharedData));
    exit(0);
}

int main() {
    printf("=======================================================\n");
    printf("  Monopoly-OS Integration Test\n");
    printf("  Testing: Scheduler + Shared Memory + Sync\n");
    printf("=======================================================\n\n");
    
    const char *shm_name = "/monopoly_test";
    
    // ========== STEP 1: Initialize Shared Memory ==========
    printf("STEP 1: Initializing shared memory...\n");
    if (init_shared_memory(shm_name, sizeof(SharedData)) == -1) {
        fprintf(stderr, "❌ Failed to initialize shared memory\n");
        return 1;
    }
    printf("✅ Shared memory initialized\n\n");
    
    // ========== STEP 2: Initialize Scheduler ==========
    printf("STEP 2: Initializing scheduler with %d players...\n", NUM_TEST_PLAYERS);
    if (scheduler_init(NUM_TEST_PLAYERS) == -1) {
        fprintf(stderr, "❌ Failed to initialize scheduler\n");
        clean_shared_memory(shared_mem_ptr, shm_name, sizeof(SharedData));
        return 1;
    }
    printf("✅ Scheduler initialized\n\n");
    
    // ========== STEP 3: Start Scheduler Thread ==========
    printf("STEP 3: Starting scheduler thread...\n");
    pthread_t scheduler_tid = scheduler_start();
    if (scheduler_tid == 0) {
        fprintf(stderr, "❌ Failed to start scheduler thread\n");
        scheduler_cleanup();
        clean_shared_memory(shared_mem_ptr, shm_name, sizeof(SharedData));
        return 1;
    }
    printf("✅ Scheduler thread started\n\n");
    
    // ========== STEP 4: Connect Players ==========
    printf("STEP 4: Connecting %d players...\n", NUM_TEST_PLAYERS);
    for (int i = 0; i < NUM_TEST_PLAYERS; i++) {
        if (scheduler_player_connect(i) == -1) {
            fprintf(stderr, "❌ Failed to connect player %d\n", i);
            scheduler_end_game();
            scheduler_stop(scheduler_tid);
            scheduler_cleanup();
            clean_shared_memory(shared_mem_ptr, shm_name, sizeof(SharedData));
            return 1;
        }
        printf("✅ Player %d connected\n", i);
    }
    printf("\n");
    
    // Give scheduler thread time to initialize
    sleep(1);
    
    // ========== STEP 5: Fork Player Processes ==========
    printf("STEP 5: Forking player processes...\n");
    pid_t pids[NUM_TEST_PLAYERS];
    
    for (int i = 0; i < NUM_TEST_PLAYERS; i++) {
        pids[i] = fork();
        
        if (pids[i] == 0) {
            // Child process
            player_process(i, shm_name);
            // Should never reach here
            exit(0);
        } else if (pids[i] < 0) {
            fprintf(stderr, "❌ Failed to fork player %d\n", i);
            // Kill already forked children
            for (int j = 0; j < i; j++) {
                kill(pids[j], SIGTERM);
            }
            scheduler_end_game();
            scheduler_stop(scheduler_tid);
            scheduler_cleanup();
            clean_shared_memory(shared_mem_ptr, shm_name, sizeof(SharedData));
            return 1;
        }
        printf("✅ Player %d process forked (PID: %d)\n", i, pids[i]);
    }
    printf("\n");
    
    // ========== STEP 6: Wait for Players to Finish ==========
    printf("STEP 6: Waiting for players to complete their turns...\n");
    printf("-------------------------------------------------------\n\n");
    
    // Let the game run for a while
    sleep(NUM_TEST_ROUNDS * 2 + 2);
    
    printf("\n-------------------------------------------------------\n");
    printf("STEP 7: Ending game...\n");
    scheduler_end_game();
    
    // Wait for all child processes
    for (int i = 0; i < NUM_TEST_PLAYERS; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (WIFEXITED(status)) {
            printf("✅ Player %d process exited with status %d\n", 
                   i, WEXITSTATUS(status));
        }
    }
    printf("\n");
    
    // ========== STEP 8: Display Results ==========
    printf("STEP 8: Displaying results from shared memory...\n");
    printf("-------------------------------------------------------\n");
    for (int i = 0; i < NUM_TEST_PLAYERS; i++) {
        printf("Player %d: %s (messages sent: %d)\n", 
               i, shared_mem_ptr->buffer[i], shared_mem_ptr->counter[i]);
    }
    printf("-------------------------------------------------------\n\n");
    
    // Display scheduler stats
    printf("Scheduler Statistics:\n");
    printf("  Total rounds: %d\n", scheduler_get_round());
    printf("  Total moves: %ld\n", scheduler_get_total_moves());
    printf("\n");
    
    // ========== STEP 9: Cleanup ==========
    printf("STEP 9: Cleaning up...\n");
    
    if (scheduler_stop(scheduler_tid) == -1) {
        fprintf(stderr, "❌ Failed to stop scheduler thread\n");
    } else {
        printf("✅ Scheduler thread stopped\n");
    }
    
    if (scheduler_cleanup() == -1) {
        fprintf(stderr, "❌ Failed to cleanup scheduler\n");
    } else {
        printf("✅ Scheduler cleaned up\n");
    }
    
    clean_shared_memory(shared_mem_ptr, shm_name, sizeof(SharedData));
    
    printf("\n=======================================================\n");
    printf("  ✅ Integration Test Complete!\n");
    printf("=======================================================\n");
    
    return 0;
}
