#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <pthread.h>
#include <semaphore.h>

#define MAX_PLAYERS 5
#define MIN_PLAYERS 3
#define BOARD_SIZE 20
#define START_MONEY 500   // Lower starting money for faster bankruptcies
#define SHM_NAME "/monopoly_game_shm"
#define SCORES_FILE "scores.txt"

// Game states
typedef enum {
    WAITING,
    PLAYING,
    GAME_OVER
} GameStatus;

// Message types
typedef enum {
    MSG_WAIT,
    MSG_YOUR_TURN,
    MSG_UPDATE,
    MSG_WIN,
    MSG_LOSE
} MessageType;

// Property structure
typedef struct {
    char name[32];
    int price;
    int rent;
    int owner;  // -1 = unowned, 0-4 = player id
} Property;

// Player structure
typedef struct {
    int id;
    int position;
    int money;
    int is_active;
    int is_bankrupt;
} Player;

// Player score tracking
typedef struct {
    char name[32];
    int wins;
    int games_played;
} PlayerScore;

// Message packet structure
typedef struct {
    MessageType type;
    int player_id;
    int position;
    int money;
    char message[256];
} Packet;

// Main game state (shared memory structure)
typedef struct {
    // Synchronization primitives (MUST be process-shared)
    pthread_mutex_t game_mutex;
    pthread_mutex_t score_mutex;
    pthread_cond_t turn_cond;
    sem_t log_sem;
    
    // Game state
    GameStatus game_state;
    int num_players;
    int active_player_count;
    int current_turn;
    int round;
    
    // Players
    Player players[MAX_PLAYERS];
    
    // Board
    Property board[BOARD_SIZE];
    
    // Persistent scores
    PlayerScore scores[MAX_PLAYERS];
    int total_games;
    
} GameState;

// Function declarations
GameState* init_game_state_memory(void);
GameState* attach_game_state_memory(void);
void cleanup_game_state_memory(GameState *state);
void load_scores(GameState *state);
void save_scores(GameState *state);
void init_board(GameState *state);
void advance_turn(GameState *state);
int get_winner(GameState *state);

#endif // GAME_STATE_H
