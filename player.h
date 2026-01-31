// player.h
#ifndef PLAYER_H
#define PLAYER_H

#define MAX_PROPERTIES 20   // Maximum properties a player can own

typedef struct {
    int id;             // Player ID
    int position;       // Current board position
    int money;          // Current money
    int skip_turn;      // 1 = skip next turn
    int active;         // 1 = still playing, 0 = bankrupt
    int properties[MAX_PROPERTIES]; // Owned properties
} Player;

#endif
