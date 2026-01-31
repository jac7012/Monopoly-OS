#include <stdio.h>
#include <stdlib.h>
#include "game_logic.h"

// Roll a dice: returns number between 1 and 6
int roll_dice() {
    return (rand() % 6) + 1;
}

// Move the player along the board
void move_player(Player* player, int steps) {
    player->position = (player->position + steps) % BOARD_SIZE;
}

// Handle landing events for the player
void handle_landing(Player* player, Property board[]) {
    Property* p = &board[player->position];

    // If unowned and player has enough money, buy property
    if(p->owner == -1 && player->money >= p->price) {
        player->money -= p->price;
        p->owner = player->id;
        printf("Player %d buys property %d for %d$\n", player->id, p->id, p->price);
    }
    // If owned by another player, pay rent
    else if(p->owner != -1 && p->owner != player->id) {
        int rent = p->rent;
        player->money -= rent;
        printf("Player %d pays %d$ rent to player %d\n", player->id, rent, p->owner);
    }

    // Random events to make gameplay fun
    int event = rand() % 10;
    if(event == 0) {
        player->money += 10;
        printf("Lucky! Player %d gains 10$\n", player->id);
    } else if(event == 1) {
        player->money -= 5;
        printf("Unlucky! Player %d loses 5$\n", player->id);
    } else if(event == 2) {
        player->skip_turn = 1;
        printf("Player %d will skip next turn!\n", player->id);
    } else if(event == 3) {
        player->position = (player->position + BOARD_SIZE - 3) % BOARD_SIZE;
        printf("Player %d moves back 3 steps!\n", player->id);
    }
}

// Check if the player is bankrupt
void check_player_status(Player* player) {
    if(player->money <= 0) {
        player->active = 0;
        printf("Player %d is bankrupt!\n", player->id);
    }
}

// Display board and player info
void display_board(Player* player, Property board[]) {
    printf("Player %d: Money: %d Position: %d\n", player->id, player->money, player->position);
    display_board_view(board);
}
