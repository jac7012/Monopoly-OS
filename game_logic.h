#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

// Need type definitions from game_state header
#include "game_state.h"

// Result of a landing event
typedef struct {
    char message[256];
    int money_change;
    int owner_id;           // For rent: who gets paid
    int property_bought;    // 1 if property was bought
    int is_bankrupt;        // 1 if player went bankrupt
} LandingResult;

// Roll a dice (1 to 6) with seed
int roll_dice_seeded(unsigned int *seed);

// Handle landing on a position - returns what happened
LandingResult handle_landing_on_position(int position, int player_id, int current_money, 
                                          Property board[], unsigned int *seed);

// Check if player is bankrupt
int is_player_bankrupt(int money);


#endif
