README
HOW TO COMPILE AND RUN

COMPILATION:
  1.Clean previous builds:
       make clean
  2. Compile the project:
       make

  This produces two executables:
    - monopoly_server (server executable)
    - monopoly_client (client executable)

RUNNING THE GAME:
  1. Start the server:
       Terminal 1: ./monopoly_server
       
  2. Start clients (3-5 players required):
       Terminal 2: ./monopoly_client
       Terminal 3: ./monopoly_client
       Terminal 4: ./monopoly_client
       (Optional terminals 5-6 for 4th and 5th players)
       
  3. Game automatically starts when minimum 3 players connect

# View game logs (optional, in separate terminal)
$ tail -f game.log

# View persistent scores (after game ends)
$ cat scores.txt


GAME RULES SUMMARY

OBJECTIVE:
  Be the last player standing (all other players bankcrupt)

SETUP:
  - Board with 20 spaces including properties and special tiles
  - Each player starts with $500
  - Players start at position 0 ("Go")
  - 3-5 players required to start game

GAMEPLAY:
  - Turn-based gameplay using Round Robin scheduling
  - On your turn, press 'r' to roll dice (1-6)
  - Move forward the number of spaces shown on dice
  - Landing on properties:
      * Unowned: Buy for listed price
      * Owned by other player: Pay rent to owner
      * Owned by you: No action
  - Special spaces:
      * Community Chest: Random event
      * Tax spaces: Pay tax amount

WINNING/LOSING:
  - Player goes bankrupt when money reaches $0 or below (negative)
  - Bankrupt players are eliminated from the game
  - Last player remaining wins
  - Wins are recorded to scores.txt

PROPERTIES:
  - 20 board spaces total
  - Properties range in price from $100 to $480
  - Rent ranges from $10 to $105
  - Property examples: Pasar Seni, Batu Caves, George Town, Melaka Old Town,
    Cameron Highlands, KLCC, Genting Highlands, Johor Bahru, Mount Kinabalu


MULTIPLAYER MODE (Client-Server Architecture):
  
  Player Requirements:
    - Minimum: 3 
    - Maximum: 5 
    - Game auto-starts when minimum players connect
    
  Network:
    - TCP/IP sockets (IPv4)
    - Default port: 8080
    
  Concurrency Model:
    - Hybrid multiprocessing and multithreading
    - Server forks child process for each client connection
    - Dedicated logger thread for event logging
    - Dedicated scheduler thread for turn management
    - Process-shared synchronization (mutexes, condition variables, semaphores)
    
  Features:
    - Server-enforced game rules (no client-side logic)
    - All dice rolls generated on server
    - Concurrent thread-safe logging to game.log
    - Persistent scoring across games (scores.txt)
    - Automatic handling of player disconnections

