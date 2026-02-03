#include "game_state.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// Initialize shared memory
GameState* init_shared_memory(void) {
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return NULL;
    }
    
    if (ftruncate(shm_fd, sizeof(GameState)) == -1) {
        perror("ftruncate");
        close(shm_fd);
        return NULL;
    }
    
    GameState *state = mmap(NULL, sizeof(GameState), PROT_READ | PROT_WRITE,
                            MAP_SHARED, shm_fd, 0);
    if (state == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return NULL;
    }
    
    close(shm_fd);
    
    // Initialize process-shared mutex
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

// Attach to existing shared memory (for child processes)
GameState* attach_shared_memory(void) {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open (attach)");
        return NULL;
    }
    
    GameState *state = mmap(NULL, sizeof(GameState), PROT_READ | PROT_WRITE,
                            MAP_SHARED, shm_fd, 0);
    if (state == MAP_FAILED) {
        perror("mmap (attach)");
        close(shm_fd);
        return NULL;
    }
    
    close(shm_fd);
    return state;
}

// Cleanup shared memory
void cleanup_shared_memory(GameState *state) {
    if (state) {
        pthread_mutex_destroy(&state->game_mutex);
        pthread_mutex_destroy(&state->score_mutex);
        pthread_cond_destroy(&state->turn_cond);
        sem_destroy(&state->log_sem);
        munmap(state, sizeof(GameState));
    }
    shm_unlink(SHM_NAME);
}

// Initialize board with properties
void init_board(GameState *state) {
    const char *property_names[BOARD_SIZE] = {
        "Go", "Mediterranean Ave", "Community Chest", "Baltic Ave",
        "Income Tax", "Reading Railroad", "Oriental Ave", "Chance",
        "Vermont Ave", "Connecticut Ave", "Jail", "St. Charles Place",
        "Electric Company", "States Ave", "Virginia Ave", "Pennsylvania RR",
        "St. James Place", "Community Chest", "Tennessee Ave", "New York Ave"
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
    
    // Check win condition - only end game if only 1 player remains solvent
    int solvent_count = 0;
    int last_solvent = -1;
    for (int i = 0; i < state->num_players; i++) {
        if (!state->players[i].is_bankrupt) {
            solvent_count++;
            last_solvent = i;
        }
    }
    
    // Check if only 1 player remains solvent OR max rounds reached
    if ((solvent_count == 1 && last_solvent >= 0) || state->round >= 50) {
        if (solvent_count == 1 && last_solvent >= 0) {
            state->game_state = GAME_OVER;
            pthread_mutex_lock(&state->score_mutex);
            state->scores[last_solvent].wins++;
            for (int i = 0; i < state->num_players; i++) {
                state->scores[i].games_played++;
            }
            state->total_games++;
            pthread_mutex_unlock(&state->score_mutex);
            logger_log("Game over! Player %d wins after %d rounds!", last_solvent, state->round);
        } else {
            // Max rounds reached - find richest player as winner
            state->game_state = GAME_OVER;
            int richest = 0;
            int max_money = shm->players[0].money;
            for (int i = 1; i < state->num_players; i++) {
                if (shm->players[i].money > max_money) {
                    max_money = shm->players[i].money;
                    richest = i;
                }
            }
            pthread_mutex_lock(&state->score_mutex);
            state->scores[richest].wins++;
            for (int i = 0; i < state->num_players; i++) {
                state->scores[i].games_played++;
            }
            state->total_games++;
            pthread_mutex_unlock(&state->score_mutex);
            logger_log("Game timeout at round %d! Richest player %d wins with $%d", state->round, richest, max_money);
        }
        save_scores(state);
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
