#include <stdio.h>
#include "board.h"

// Initialize the board properties
void init_board(Property board[]) {
    for(int i = 0; i < BOARD_SIZE; i++) {
        board[i].id = i;
        board[i].price = 100 + i * 20;   // Significant purchase cost: 100-480
        board[i].rent = 50 + i * 10;     // Meaningful rent: 50-240
        board[i].owner = -1;              // Initially no owner
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
