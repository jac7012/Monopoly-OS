#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "player.h"
#include "board.h"

// Roll a dice (1 to 6)
int roll_dice();

// Move player by steps
void move_player(Player* player, int steps);

// Handle what happens when player lands on a property
void handle_landing(Player* player, Property board[]);

// Check if player is bankrupt or should skip turn
void check_player_status(Player* player);

// Display the board and player's info
void display_board(Player* player, Property board[]);

#endif
