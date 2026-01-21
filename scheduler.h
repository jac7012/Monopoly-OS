#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

/**
 * Scheduler Module Header
 * 
 * Implements Round Robin (RR) scheduling for managing player turns in the
 * Monopoly game. The scheduler is a dedicated thread in the parent process
 * that:
 * 
 * - Maintains cyclic player order
 * - Determines whose turn it is
 * - Signals when a player may act
 * - Skips disconnected/inactive players
 * - Ensures only one player acts at a time
 * 
 * Turn state resides in shared memory and is protected by synchronization
 * primitives to ensure safe access from parent threads and fork'd children.
 */

#define MAX_PLAYERS 5
#define MIN_PLAYERS 3

/**
 * Represents a single player in the scheduling system
 */
typedef struct {
    int player_id;           // Unique player identifier (0 to num_players-1)
    bool is_connected;       // True if player is actively connected
    bool is_active;          // True if player is eligible to take turns
    int turn_count;          // Number of turns this player has taken
    pthread_t client_thread; // Thread/process ID of the client (if applicable)
} Player;

/**
 * Central scheduler state (shared memory structure)
 * 
 * This structure is placed in shared memory to ensure:
 * - Parent scheduler thread can read/write
 * - Child processes can read current turn state
 * - All access is synchronized via scheduler_lock
 */
typedef struct {
    // Player information
    Player players[MAX_PLAYERS];
    int num_players;              // Current number of players (3-5)
    int active_player_count;      // Number of currently active players
    
    // Turn management
    int current_player_idx;       // Index of player whose turn it is (0 to num_players-1)
    int round_number;             // Current round/cycle number
    long total_moves;             // Total moves made in this game
    
    // Synchronization (protected by these primitives)
    pthread_mutex_t scheduler_lock;      // Protects all scheduler state
    sem_t turn_signal[MAX_PLAYERS];      // Per-player turn signals (only current player's is non-zero)
    pthread_cond_t turn_changed;         // Condition variable for turn changes
    
    // Control flags
    bool game_in_progress;        // True while game is active
    bool scheduler_running;       // True while scheduler thread is active
} SchedulerState;


/**
 * Initialize the scheduler state in shared memory
 * 
 * Must be called once during server startup before any game starts.
 * Initializes all players as disconnected, sets up synchronization primitives,
 * and prepares for the first game.
 * 
 * @param num_players Number of players for this game (3-5)
 * @return 0 on success, -1 on failure
 */
int scheduler_init(int num_players);

/**
 * Start the scheduler thread
 * 
 * Creates a new thread that runs the Round Robin scheduling loop.
 * This thread manages turn advancement and enforces fairness.
 * Call after scheduler_init() and before the game starts.
 * 
 * @return Thread ID on success, 0 on failure
 */
pthread_t scheduler_start(void);

/**
 * Register a player as connected
 * 
 * Called when a client connects to the server.
 * Marks the player as connected and eligible for turns.
 * 
 * @param player_id Player ID (0 to num_players-1)
 * @return 0 on success, -1 on failure (invalid player_id, already connected, etc.)
 */
int scheduler_player_connect(int player_id);

/**
 * Unregister a player as disconnected
 * 
 * Called when a client disconnects or becomes inactive.
 * The scheduler will skip this player's turns in the future.
 * 
 * @param player_id Player ID
 * @return 0 on success, -1 on failure
 */
int scheduler_player_disconnect(int player_id);

/**
 * Get the current player's ID
 * 
 * Safe to call from any thread or child process.
 * Returns the ID of the player whose turn it currently is.
 * 
 * @return Player ID (0 to num_players-1), or -1 on error
 */
int scheduler_get_current_player(void);

/**
 * Check if it's a specific player's turn
 * 
 * Safe for child processes to check if they should act.
 * Prevents players from acting out of turn.
 * 
 * @param player_id Player ID to check
 * @return 1 if it's this player's turn, 0 if not, -1 on error
 */
int scheduler_is_my_turn(int player_id);

/**
 * Get the total number of players
 * 
 * @return Number of players (3-5)
 */
int scheduler_get_num_players(void);

/**
 * Get the current round number
 * 
 * @return Round number (0-based)
 */
int scheduler_get_round(void);

/**
 * Get total moves made so far
 * 
 * @return Total move count
 */
long scheduler_get_total_moves(void);

/**
 * Advance to the next player's turn
 * 
 * Called by the scheduler thread or by external game logic to move
 * to the next eligible player. Skips disconnected players automatically.
 * 
 * @return ID of the new current player, or -1 on failure
 */
int scheduler_advance_turn(void);

/**
 * Request the scheduler to end the current game
 * 
 * Sets game_in_progress to false, causing the scheduler to exit cleanly.
 * 
 * @return 0 on success, -1 on failure
 */
int scheduler_end_game(void);

/**
 * Stop the scheduler thread
 * 
 * Waits for the scheduler thread to terminate gracefully.
 * Call this during server shutdown.
 * 
 * @param scheduler_tid Thread ID returned by scheduler_start()
 * @return 0 on success, -1 on failure
 */
int scheduler_stop(pthread_t scheduler_tid);

/**
 * Cleanup and destroy the scheduler state
 * 
 * Destroys all synchronization primitives and unmaps shared memory.
 * Call during server shutdown.
 * 
 * @return 0 on success, -1 on failure
 */
int scheduler_cleanup(void);

/* ============================================================================
 * SCHEDULER THREAD MAIN LOOP
 * ============================================================================ */

/**
 * Main scheduler thread function
 * 
 * Runs as a separate thread in the parent process.
 * Continuously manages player turns, ensuring only one player acts at a time.
 * 
 * This is NOT meant to be called directly; use scheduler_start() instead.
 * 
 * @param arg Unused (required by pthread API)
 * @return NULL
 */
void *scheduler_thread_main(void *arg);

/* ============================================================================
 * ACCESSOR FUNCTIONS FOR SHARED STATE
 * ============================================================================ */

/**
 * Get reference to the scheduler state
 * 
 * CAUTION: Direct access requires proper synchronization!
 * Use scheduler_lock before reading/modifying.
 * 
 * @return Pointer to SchedulerState in shared memory, or NULL on error
 */
SchedulerState *scheduler_get_state(void);

#endif // SCHEDULER_H
