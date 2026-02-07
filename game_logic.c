#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game_logic.h"

// Roll a dice with seed: returns number between 1 and 6
int roll_dice_seeded(unsigned int *seed) {
    return (rand_r(seed) % 6) + 1;
}

// Check if player is bankrupt
int is_player_bankrupt(int money) {
    return money < 0;
}

// Handle landing on a position - returns what happened
LandingResult handle_landing_on_position(int position, int player_id, int current_money,
                                          Property board[], unsigned int *seed) {
    LandingResult result;
    memset(&result, 0, sizeof(LandingResult));
    result.money_change = 0;
    result.owner_id = -1;
    result.property_bought = 0;
    result.is_bankrupt = 0;
    
    // TAX OFFICE (position 10): Pay $50 tax
    if (position == 10) {
        result.money_change = -50;
        sprintf(result.message, "TAX OFFICE! You paid $50 in taxes");
    }
    // COMMUNITY CHEST (positions 2, 17): Random card draw
    else if (position == 2 || position == 17) {
        int card = rand_r(seed) % 2;  // Random card effect
        if (card == 0) {
            // Good luck card - get money
            int bonus = 100 + (rand_r(seed) % 50);
            result.money_change = bonus;
            sprintf(result.message, "Community Chest! You drew a LUCKY card: +$%d!", bonus);
        } else {
            // Bad luck card - pay money
            int penalty = 50 + (rand_r(seed) % 50);
            result.money_change = -penalty;
            sprintf(result.message, "Community Chest! You drew a BAD card: -$%d!", penalty);
        }
    }
    // NORMAL PROPERTY
    else {
        Property *prop = &board[position];
        
        if (prop->owner == -1) {
            // Unowned - buy if can afford
            if (current_money >= prop->price) {
                result.money_change = -prop->price;
                result.property_bought = 1;
                sprintf(result.message, "Bought %s for $%d", prop->name, prop->price);
            } else {
                sprintf(result.message, "Can't afford %s ($%d needed)", prop->name, prop->price);
            }
        } else if (prop->owner != player_id) {
            // Pay rent
            int rent = prop->rent;
            result.money_change = -rent;
            result.owner_id = prop->owner;
            sprintf(result.message, "Paid $%d rent to Player %d on %s", rent, prop->owner, prop->name);
        } else {
            sprintf(result.message, "Landed on own property %s", prop->name);
        }
    }
    
    // Check if player would go bankrupt
    int new_money = current_money + result.money_change;
    if (is_player_bankrupt(new_money)) {
        result.is_bankrupt = 1;
        strcat(result.message, " - BANKRUPT!");
    }
    
    return result;
}
<<<<<<< HEAD


=======
>>>>>>> 9eab809df4a66e2b6187c91dbcb000bddc2d27ae
