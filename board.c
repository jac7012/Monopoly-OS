#include <stdio.h>
#include "board.h"

// Initialize the board properties
void init_board(Property board[]) {
    for(int i = 0; i < BOARD_SIZE; i++) {
        board[i].id = i;
        board[i].price = 20 + i * 5;  // Example price increment
        board[i].rent = 5 + i * 2;    // Example rent increment
        board[i].owner = -1;           // Initially no owner
    }
}

// Display the board in a simple text format
void display_board_view(Property board[]) {
    printf("Board View:\n");
    for(int i = 0; i < BOARD_SIZE; i++) {
        printf("[%d: Price:%d$ Owner:%d] ", board[i].id, board[i].price, board[i].owner);
    }
    printf("\n");
}
