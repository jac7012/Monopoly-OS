#ifndef BOARD_H
#define BOARD_H

#define BOARD_SIZE 20   // Total number of properties on the board

typedef struct Property {
    int id;      // Unique property ID
    int price;   // Price to buy the property
    int rent;    // Rent to pay if owned by another player
    int owner;   // -1 = unowned, otherwise player ID
} Property;

// Initialize the board with prices, rents, and no owners
void init_board(Property board[]);

// Display the current board with owners and prices
void display_board_view(Property board[]);

#endif
