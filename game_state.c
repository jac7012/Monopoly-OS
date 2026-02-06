#include "game_state.h"
#include "shared_memory.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

// Initializes GameState within the SharedData structure
GameState* init_game_state_memory(void) {
    // Call shared_memory.c's init_shared_memory function
    if (init_shared_memory(SHM_NAME, sizeof(SharedData)) != 0) {
        logger_log("Failed to initialize shared memory");
        return NULL;
    }
    
    // Now initialize GameState fields on top of the shared memory
    GameState *state = (GameState *)shared_mem_ptr;
    
    // Initialize process-shared synchronization primitives
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&state->game_mutex, &mutex_attr);
    pthread_mutex_init(&state->score_mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);
    
    // Initialize process-shared condition variable
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&state->turn_cond, &cond_attr);
    pthread_condattr_destroy(&cond_attr);
    
    // Initialize semaphore for logging
    sem_init(&state->log_sem, 1, 1);  // 1 = process-shared
    
    // Initialize game state
    state->game_state = WAITING;
    state->num_players = 0;
    state->active_player_count = 0;
    state->current_turn = 0;
    state->round = 0;
    state->total_games = 0;
    
    // Initialize board
    init_board(state);
    
    // Initialize player scores
    for (int i = 0; i < MAX_PLAYERS; i++) {
        snprintf(state->scores[i].name, sizeof(state->scores[i].name), 
                "Player %d", i);
        state->scores[i].wins = 0;
        state->scores[i].games_played = 0;
    }
    
    return state;
}

// Wrapper: Attach to existing shared memory (for child processes)
// Uses shared_memory.c to attach, then returns as GameState*
GameState* attach_game_state_memory(void) {
    SharedData *shm = attach_shared_memory(SHM_NAME, sizeof(SharedData));
    if (shm == NULL) {
        logger_log("Failed to attach to shared memory");
        return NULL;
    }
    return (GameState *)shm;
}

// Wrapper: Cleanup shared memory
// Destroys synchronization primitives and calls shared_memory.c cleanup
void cleanup_game_state_memory(GameState *state) {
    if (state) {
        pthread_mutex_destroy(&state->game_mutex);
        pthread_mutex_destroy(&state->score_mutex);
        pthread_cond_destroy(&state->turn_cond);
        sem_destroy(&state->log_sem);
    }
    // Use shared_memory.c's cleanup function
    clean_shared_memory((SharedData *)state, SHM_NAME, sizeof(SharedData));
}

// Initialize board with properties
void init_board(GameState *state) {
    const char *property_names[BOARD_SIZE] = {
        "Go", "Pasar Seni", "Community Chest", "Batu Caves",
        "Income Tax", "KL Sentral", "George Town", "Chance",
        "Langkawi", "Penang Hill", "Tax Office", "Melaka Old Town",
        "TNB HQ", "Putrajaya", "Cameron Highlands", "KLCC",
        "Genting Highlands", "Community Chest", "Johor Bahru", "Mount Kinabalu"
    };
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        strncpy(state->board[i].name, property_names[i], 31);
        state->board[i].name[31] = '\0';
        state->board[i].price = 100 + (i * 20);
        state->board[i].rent = 10 + (i * 5);
        state->board[i].owner = -1;
    }
}

// Load scores from file
void load_scores(GameState *state) {
    FILE *f = fopen(SCORES_FILE, "r");
    if (!f) {
        return;  // File doesn't exist yet
    }
    
    pthread_mutex_lock(&state->score_mutex);
    
    fscanf(f, "Total Games: %d\n", &state->total_games);
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        int id, wins, games;
        char name[32];
        if (fscanf(f, "Player %d: %31s - %d wins / %d games\n", 
                   &id, name, &wins, &games) == 4) {
            state->scores[i].wins = wins;
            state->scores[i].games_played = games;
            strncpy(state->scores[i].name, name, 31);
            state->scores[i].name[31] = '\0';
        }
    }
    
    pthread_mutex_unlock(&state->score_mutex);
    fclose(f);
}

// Save scores to file (atomic with mutex protection)
void save_scores(GameState *state) {
    pthread_mutex_lock(&state->score_mutex);
    
    FILE *f = fopen(SCORES_FILE, "w");
    if (!f) {
        pthread_mutex_unlock(&state->score_mutex);
        return;
    }
    
    fprintf(f, "Total Games: %d\n", state->total_games);
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        fprintf(f, "Player %d: %s - %d wins / %d games\n",
                i, state->scores[i].name,
                state->scores[i].wins,
                state->scores[i].games_played);
    }
    
    fclose(f);
    pthread_mutex_unlock(&state->score_mutex);
}

// Advance to next active player's turn
void advance_turn(GameState *state) {
    int attempts = 0;
    do {
        state->current_turn = (state->current_turn + 1) % state->num_players;
        attempts++;
    } while (state->players[state->current_turn].is_bankrupt && 
             attempts < MAX_PLAYERS);
    
    // Check if we completed a round
    if (state->current_turn == 0) {
        state->round++;
    }
    
    // Check win condition
    int active_count = 0;
    int winner_id = -1;
    for (int i = 0; i < state->num_players; i++) {
        if (!state->players[i].is_bankrupt && state->players[i].is_active) {
            active_count++;
            winner_id = i;
        }
    }
    
    if (active_count <= 1) {
        state->game_state = GAME_OVER;
        if (winner_id >= 0) {
            pthread_mutex_lock(&state->score_mutex);
            state->scores[winner_id].wins++;
            for (int i = 0; i < state->num_players; i++) {
                state->scores[i].games_played++;
            }
            state->total_games++;
            pthread_mutex_unlock(&state->score_mutex);
            logger_log("Game over! Player %d wins!", winner_id);
            save_scores(state);
        }
    }
}

// Get winner ID
int get_winner(GameState *state) {
    for (int i = 0; i < state->num_players; i++) {
        if (!state->players[i].is_bankrupt && state->players[i].is_active) {
            return i;
        }
    }
    return -1;
}
