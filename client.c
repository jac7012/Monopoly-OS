#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "player.h"
#include "board.h"
#include "game_logic.h"
#include "client_network.h" // Handles server communication (IPC / socket)

// Main gameplay loop
void play_game(Player* player, Property board[]) {
    while(player->active) {
        // Wait for server permission to take turn
        wait_for_turn_signal(player);

        if(player->skip_turn) {
            printf("Player %d skips this turn.\n", player->id);
            player->skip_turn = 0;
            continue;
        }

        // Roll dice
        int steps = roll_dice();
        printf("Player %d rolls: %d\n", player->id, steps);

        // Move player
        move_player(player, steps);
        printf("Player %d moves to position %d\n", player->id, player->position);

        // Handle landing events: buy, rent, random events
        handle_landing(player, board);

        // Check if player is bankrupt
        check_player_status(player);

        // Send updated state to server
        send_state_to_server(player);

        // Receive updated board from server
        receive_update_from_server(player, board);

        // Display board and player info
        display_board(player, board);

        usleep(200000); // Slight delay for readability
    }

    printf("Player %d is out of the game.\n", player->id);
}

// Main entry point
int main(int argc, char* argv[]) {
    srand(time(NULL) + getpid()); // Seed RNG uniquely per client

    Player player;
    player.id = (argc > 1) ? atoi(argv[1]) : 1;
    player.money = 100;
    player.position = 0;
    player.active = 1;
    player.skip_turn = 0;
    for(int i = 0; i < MAX_PROPERTIES; i++) player.properties[i] = -1;

    Property board[BOARD_SIZE];
    init_board(board);

    // Connect to the server (IPC or socket)
    if(connect_to_server() != 0) {
        printf("Failed to connect to server.\n");
        return -1;
    }

    // Start gameplay loop
    play_game(&player, board);

    // Disconnect from server when done
    disconnect_from_server();
    return 0;
}
