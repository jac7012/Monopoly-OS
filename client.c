#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "game_state.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

int main(int argc, char *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    Packet pkt;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    // Convert IP address
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/Address not supported\n");
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed\n");
        return -1;
    }

    printf("Connected to Monopoly Server!\n");
    printf("Waiting for game to start...\n\n");

    // Main game loop
    while (1) {
        // Receive packet from server
        int valread = read(sock, &pkt, sizeof(Packet));
        if (valread <= 0) {
            printf("Server disconnected.\n");
            break;
        }

        switch (pkt.type) {
            case MSG_WAIT:
                printf("Waiting for more players...\n");
                break;

            case MSG_YOUR_TURN:
                printf("\n========================================\n");
                printf("YOUR TURN!\n");
                printf("Position: %d | Money: $%d\n", pkt.position, pkt.money);
                printf("%s\n", pkt.message);
                printf("========================================\n");
                printf("Press 'r' and Enter to roll dice: ");
                
                char input;
                scanf(" %c", &input);
                write(sock, &input, 1);
                break;

            case MSG_UPDATE:
                printf("\n[UPDATE] %s\n", pkt.message);
                printf("New Position: %d | Money: $%d\n", pkt.position, pkt.money);
                break;

            case MSG_WIN:
                printf("\n");
                printf("****************************************\n");
                printf("*                                      *\n");
                printf("*     CONGRATULATIONS! YOU WIN!        *\n");
                printf("*                                      *\n");
                printf("****************************************\n");
                printf("Final Money: $%d\n", pkt.money);
                printf("%s\n", pkt.message);
                close(sock);
                return 0;

            case MSG_LOSE:
                printf("\n");
                printf("========================================\n");
                printf("         GAME OVER - YOU LOST          \n");
                printf("========================================\n");
                printf("Final Money: $%d\n", pkt.money);
                printf("%s\n", pkt.message);
                close(sock);
                return 0;

            default:
                printf("Unknown message type received\n");
                break;
        }
    }

    close(sock);
    return 0;
}
