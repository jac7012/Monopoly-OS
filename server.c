#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdint.h>
#include "game_state.h"
#include "logger.h"
#include "scheduler.h"

#define PORT 8080
#define MAX_CLIENTS 5
#define MIN_CLIENTS 3

// Global server state
int server_fd;
GameState *game_state = NULL;
pthread_t logger_thread_id;
pthread_t scheduler_thread_id;

// Signal handler for graceful shutdown
void sig_handler(int signo) {
    if (signo == SIGINT) {
        printf("\n[SERVER] Shutting down gracefully...\n");
        save_scores(game_state);
        logger_log("Server shutdown requested");
        logger_shutdown();
        
        if (game_state) {
            cleanup_game_state_memory(game_state);
        }
        close(server_fd);
        exit(0);
    }
    
    // Reap zombie processes
    if (signo == SIGCHLD) {
        while (waitpid(-1, NULL, WNOHANG) > 0) {
            logger_log("Child process reaped");
        }
    }
}

// Handle individual client in child process
void handle_client(int client_socket, int player_id) {
    char buffer[256];
    Packet pkt;
    
    logger_log("Player %d session started (PID: %d)", player_id, getpid());
    
    // Attach to shared memory
        GameState *shm = attach_game_state_memory();
    if (!shm) {
        logger_log("Player %d failed to attach shared memory", player_id);
        close(client_socket);
        exit(1);
    }
    
    // Main game loop for this client
    while (1) {
        // Wait for turn
        pthread_mutex_lock(&shm->game_mutex);
        
        // Check if game is over
        if (shm->game_state == GAME_OVER) {
            int winner_id = get_winner(shm);
            if (winner_id == player_id) {
                pkt.type = MSG_WIN;
                sprintf(pkt.message, "Congratulations! You won!");
            } else {
                pkt.type = MSG_LOSE;
                sprintf(pkt.message, "Game Over. Player %d won.", winner_id);
            }
            pkt.player_id = player_id;
            pkt.money = shm->players[player_id].money;
            write(client_socket, &pkt, sizeof(Packet));
            pthread_mutex_unlock(&shm->game_mutex);
            break;
        }
        
        // Wait until game starts AND it's this player's turn
        while ((shm->game_state != PLAYING || shm->current_turn != player_id) && 
               shm->game_state != GAME_OVER) {
            pthread_cond_wait(&shm->turn_cond, &shm->game_mutex);
        }
        
        // Check again if game ended while waiting
        if (shm->game_state == GAME_OVER) {
            pthread_mutex_unlock(&shm->game_mutex);
            continue;
        }
        
        // Skip if player is bankrupt
        if (shm->players[player_id].is_bankrupt) {
            advance_turn(shm);
            pthread_cond_broadcast(&shm->turn_cond);
            pthread_mutex_unlock(&shm->game_mutex);
            continue;
        }
        
        // Send turn notification
        pkt.type = MSG_YOUR_TURN;
        pkt.player_id = player_id;
        pkt.position = shm->players[player_id].position;
        pkt.money = shm->players[player_id].money;
        sprintf(pkt.message, "Your turn! Press 'r' to roll dice.");
        pthread_mutex_unlock(&shm->game_mutex);
        
        write(client_socket, &pkt, sizeof(Packet));
        
        // Wait for player action
        char action;
        int n = read(client_socket, &action, 1);
        if (n <= 0) {
            logger_log("Player %d disconnected", player_id);
            pthread_mutex_lock(&shm->game_mutex);
            shm->players[player_id].is_active = 0;
            shm->active_player_count--;
            pthread_mutex_unlock(&shm->game_mutex);
            break;
        }
        
        pthread_mutex_lock(&shm->game_mutex);
        
        // Initialize packet for response
        memset(&pkt, 0, sizeof(Packet));
        pkt.player_id = player_id;
        pkt.position = shm->players[player_id].position;
        pkt.money = shm->players[player_id].money;
        
        if (action == 'r' && !shm->players[player_id].is_bankrupt) {
            // Server generates dice roll - use higher precision seed for each player
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            unsigned int unique_seed = ts.tv_nsec + player_id * 12345 + (uintptr_t)&shm->players[player_id];
            int dice = (rand_r(&unique_seed) % 6) + 1;
            logger_log("Player %d rolled %d", player_id, dice);
            
            // Move player
            shm->players[player_id].position = 
                (shm->players[player_id].position + dice) % BOARD_SIZE;
            
            // Handle landing on special spaces
            int pos = shm->players[player_id].position;
            
            // TAX OFFICE (position 10): Pay $50 tax
            if (pos == 10) {
                shm->players[player_id].money -= 50;
                sprintf(pkt.message, "Rolled %d. TAX OFFICE! You paid $50 in taxes", dice);
                logger_log("Player %d landed on Tax Office - paid $50", player_id);
            }
            // COMMUNITY CHEST (positions 2, 17): Random card draw
            else if (pos == 2 || pos == 17) {
                int card = rand_r(&unique_seed) % 2;  // Random card effect
                if (card == 0) {
                    // Good luck card - get money
                    int bonus = 100 + (rand_r(&unique_seed) % 50);
                    shm->players[player_id].money += bonus;
                    sprintf(pkt.message, "Rolled %d. Community Chest! You drew a LUCKY card: +$%d!", dice, bonus);
                    logger_log("Player %d drew lucky community chest: +$%d", player_id, bonus);
                } else {
                    // Bad luck card - pay money
                    int penalty = 50 + (rand_r(&unique_seed) % 50);
                    shm->players[player_id].money -= penalty;
                    sprintf(pkt.message, "Rolled %d. Community Chest! You drew a BAD card: -$%d!", dice, penalty);
                    logger_log("Player %d drew bad community chest: -$%d", player_id, penalty);
                }
            }
            // NORMAL PROPERTY
            else {
                Property *prop = &shm->board[pos];
                
                if (prop->owner == -1) {
                    // Unowned - buy if can afford
                    if (shm->players[player_id].money >= prop->price) {
                        shm->players[player_id].money -= prop->price;
                        prop->owner = player_id;
                        sprintf(pkt.message, "Rolled %d. Bought %s for $%d", 
                               dice, prop->name, prop->price);
                        logger_log("Player %d bought %s", player_id, prop->name);
                    } else {
                        sprintf(pkt.message, "Rolled %d. Can't afford %s ($%d needed)", 
                               dice, prop->name, prop->price);
                    }
                } else if (prop->owner != player_id) {
                    // Pay rent
                    int rent = prop->rent;
                    shm->players[player_id].money -= rent;
                    shm->players[prop->owner].money += rent;
                    sprintf(pkt.message, "Rolled %d. Paid $%d rent to Player %d on %s", 
                           dice, rent, prop->owner, prop->name);
                    logger_log("Player %d paid $%d rent to Player %d", 
                              player_id, rent, prop->owner);
                } else {
                    sprintf(pkt.message, "Rolled %d. Landed on own property %s", dice, prop->name);
                }
            }
            
            // Check bankruptcy
            if (shm->players[player_id].money < 0) {
                shm->players[player_id].is_bankrupt = 1;
                shm->active_player_count--;
                logger_log("Player %d went bankrupt", player_id);
                sprintf(pkt.message + strlen(pkt.message), " - BANKRUPT!");
            }
            
            // Send update
            pkt.type = MSG_UPDATE;
            pkt.position = shm->players[player_id].position;
            pkt.money = shm->players[player_id].money;
        } else {
            // Invalid action - send current state
            pkt.type = MSG_UPDATE;
            sprintf(pkt.message, "Invalid action");
        }
        
        if (write(client_socket, &pkt, sizeof(Packet)) < 0) {
            logger_log("Player %d write failed", player_id);
            pthread_mutex_unlock(&shm->game_mutex);
            break;
        }
        
        // Advance turn
        advance_turn(shm);
        pthread_cond_broadcast(&shm->turn_cond);
        pthread_mutex_unlock(&shm->game_mutex);
    }
    
    logger_log("Player %d session ended", player_id);
    close(client_socket);
    exit(0);
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int opt = 1;
    
    srand(time(NULL));
    
    // Setup signal handlers
    signal(SIGINT, sig_handler);
    signal(SIGCHLD, sig_handler);
    
    // Initialize logger
    if (logger_init("game.log") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }
    logger_log("=== Monopoly Server Starting ===");
    
    // Initialize shared memory
        game_state = init_game_state_memory();
    if (!game_state) {
        logger_log("Failed to initialize shared memory");
        logger_shutdown();
        return 1;
    }
    
    // Load persistent scores
    load_scores(game_state);
    logger_log("Loaded scores from file");
    
    // Initialize scheduler
    if (scheduler_init(MAX_CLIENTS) != 0) {
        logger_log("Failed to initialize scheduler");
            cleanup_game_state_memory(game_state);
        logger_shutdown();
        return 1;
    }
    
    // Start scheduler thread
    scheduler_thread_id = scheduler_start();
    if (scheduler_thread_id == 0) {
        logger_log("Failed to start scheduler thread");
        scheduler_cleanup();
            cleanup_game_state_memory(game_state);
        logger_shutdown();
        return 1;
    }
    logger_log("Scheduler thread started");
    
    // Create server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }
    
    logger_log("Server listening on port %d", PORT);
    printf("[SERVER] Listening on port %d...\n", PORT);
    
    // Accept loop
    while (1) {
        int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }
        
        pthread_mutex_lock(&game_state->game_mutex);
        
        // Check if we can accept more players
        if (game_state->num_players >= MAX_CLIENTS) {
            logger_log("Connection rejected - game full");
            pthread_mutex_unlock(&game_state->game_mutex);
            close(client_socket);
            continue;
        }
        
       if (game_state->game_state == GAME_OVER) {
            logger_log("Connection rejected - game over");
            pthread_mutex_unlock(&game_state->game_mutex);
            close(client_socket);
            continue;
        }
        
        
        // Assign player ID
        int player_id = game_state->num_players++;
        game_state->players[player_id].id = player_id;
        game_state->players[player_id].money = START_MONEY;
        game_state->players[player_id].position = 0;
        game_state->players[player_id].is_active = 1;
        game_state->players[player_id].is_bankrupt = 0;
        game_state->active_player_count++;
        
        logger_log("Player %d connected from %s (Total: %d/%d)", 
                   player_id, inet_ntoa(client_addr.sin_addr), 
                   game_state->num_players, MIN_CLIENTS);
        
        // Start game if we have enough players
        if (game_state->num_players >= MIN_CLIENTS && 
            game_state->game_state == WAITING) {
            game_state->game_state = PLAYING;
            game_state->current_turn = 0;
            game_state->round = 0;
            logger_log("Game starting with %d players", game_state->num_players);
            printf("[SERVER] Game starting with %d players!\n", game_state->num_players);
            pthread_cond_broadcast(&game_state->turn_cond);
        }
        
        pthread_mutex_unlock(&game_state->game_mutex);
        
        // Fork child process to handle this client
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(server_fd);
            handle_client(client_socket, player_id);
        } else if (pid > 0) {
            // Parent process
            close(client_socket);
            scheduler_player_connect(player_id);
        } else {
            perror("fork");
            close(client_socket);
        }
    }
    
    return 0;
}
