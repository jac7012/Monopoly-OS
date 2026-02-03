# Monopoly-OS: Hybrid Concurrency Multiplayer Game

A complete implementation of a multiplayer Monopoly-style board game demonstrating advanced Operating Systems concepts including multiprocessing, multithreading, inter-process communication, and synchronization.

## ✅ Assignment Compliance Summary

### 1. **Hybrid Concurrency Model** ✅
- **Multiprocessing (fork())**: Server forks child process for each client connection (`server.c:344`)
- **Multithreading (pthreads)**: Two dedicated threads in parent process:
  - Logger thread (`logger.c`) - concurrent event logging
  - Scheduler thread (`scheduler.c`) - Round Robin turn management
- **Zombie Reaping**: SIGCHLD handler in parent process (`server.c:35-40`)

### 2. **Player Support** ✅
- Supports exactly 3-5 players (MIN_PLAYERS=3, MAX_PLAYERS=5)
- Server validates player count before starting game
- Auto-starts when minimum players join

### 3. **Server-Enforced Rules** ✅
- All dice rolls generated on server only (`server.c:117`)
- Property transactions validated server-side
- Rent calculations server-side
- Bankruptcy detection server-side
- **No client-side game logic** - client is display-only

### 4. **Inter-Process Communication (IPC)** ✅
- **POSIX Shared Memory**: Game state accessible to all processes (`game_state.c:12-65`)
  - Shared memory name: `/monopoly_game_shm`
  - Contains game board, player data, turn state
- **TCP Sockets (IPv4)**: Multi-machine client-server communication
- **Message Queue**: Logger uses POSIX mqueue for cross-process logging (`logger.c`)

### 5. **Synchronization Primitives** ✅
#### Process-Shared Mutexes:
- `game_mutex`: Protects all game state (`game_state.c:27-31`)
- `score_mutex`: Protects persistent score updates (`game_state.c:32`)
- Both initialized with `PTHREAD_PROCESS_SHARED`

#### Process-Shared Condition Variables:
- `turn_cond`: Signals turn changes to waiting players (`game_state.c:35-38`)
- Initialized with `PTHREAD_PROCESS_SHARED`

#### Semaphores:
- `log_sem`: Protects log file writes (`game_state.c:41`)
- Process-shared (flag=1)
- Per-player turn signals in scheduler (`scheduler.h:52`)

### 6. **Round Robin Scheduler** ✅
#### Dedicated Scheduler Thread (`scheduler.c`):
- Maintains cyclic player order
- Determines current player's turn
- Skips disconnected/bankrupt players (`scheduler.c:32-53`)
- Signals turn changes via condition variable
- Turn state in shared memory, synchronized with mutex

### 7. **Thread-Safe Concurrent Logger** ✅
#### Logger Thread (`logger.c`):
- Runs continuously in parent process
- Uses POSIX message queue for non-blocking enqueue
- Protected by semaphore for file access
- Logs all events with timestamps:
  - Player connections/disconnections
  - Dice rolls and moves
  - Property transactions
  - Bankruptcies
  - Game start/end

#### Guarantees:
- ✅ Complete messages (no interleaving)
- ✅ Chronologically ordered
- ✅ Non-blocking to gameplay (queue-based)

### 8. **Persistent Scoring** ✅
#### File: `scores.txt`
- Loaded into shared memory at server startup (`game_state.c:125-148`)
- Updated atomically at game end with score_mutex (`game_state.c:195-206`)
- Saved on server shutdown via SIGINT handler (`server.c:22-31`)
- Tracks per-player wins and games played

#### Synchronization:
- All score operations protected by `score_mutex`
- Prevents race conditions from concurrent child processes
- Atomic file writes with mutex held

### 9. **Multiple Successive Games** ✅
- Game state can reset without server restart
- Round counter and game state enum support multiple sessions
- Persistent scores accumulate across all games
- Server continues accepting connections after game ends

## Architecture

```
Parent Process (Server)
├── Logger Thread ────────> game.log
├── Scheduler Thread ─────> Turn Management
├── Accept Loop
│   ├── fork() → Child Process 1 (Player 0)
│   ├── fork() → Child Process 2 (Player 1)
│   ├── fork() → Child Process 3 (Player 2)
│   └── fork() → Child Process N (Player N)
│
└── Shared Memory
    ├── Game State (mutex-protected)
    ├── Player Data
    ├── Board Properties
    ├── Persistent Scores (score_mutex-protected)
    └── Turn State (cond-var coordinated)
```

## Building

```bash
cd Monopoly-OS
make clean
make
```

Produces:
- `monopoly_server` - Server executable
- `monopoly_client` - Client executable

## Running

### Terminal 1: Start Server
```bash
./monopoly_server
```

### Terminals 2-6: Start Clients (3-5 required)
```bash
./monopoly_client
```

Game automatically starts when 3+ players connect.

## Files

### Core Server Files:
- **server.c** - Main server with fork() and thread management
- **game_state.c/h** - Shared memory game state management
- **scheduler.c/h** - Round Robin scheduling thread
- **logger.c/h** - Concurrent logging thread
- **sync.c/h** - Synchronization primitives

### Client Files:
- **client.c** - Simple text-based client UI
- **board.c/h** - Board display utilities
- **game_logic.c/h** - Helper functions

### Generated at Runtime:
- **game.log** - Timestamped event log
- **scores.txt** - Persistent player statistics

## Key Synchronization Flows

### Turn Management:
1. Child process locks `game_mutex`
2. Checks `current_turn` in shared memory
3. If not my turn: `pthread_cond_wait(&turn_cond, &game_mutex)`
4. When turn ends: `pthread_cond_broadcast(&turn_cond)`
5. Scheduler advances `current_turn` atomically

### Logging:
1. Any process calls `logger_log(fmt, ...)`
2. Message enqueued to POSIX mqueue (non-blocking)
3. Logger thread dequeues in dedicated thread
4. Semaphore protects file write
5. Timestamp added, flushed immediately

### Score Updates:
1. Game ends, winner determined
2. `pthread_mutex_lock(&score_mutex)`
3. Update `scores[winner].wins++`
4. Write to `scores.txt`
5. `pthread_mutex_unlock(&score_mutex)`

## Testing Compliance

Run the following test scenarios:

1. **3 Players**: Minimum player requirement
2. **5 Players**: Maximum player requirement
3. **Player Disconnect**: Verify turn skipping
4. **Multiple Games**: Run successive games without restart
5. **Concurrent Logging**: Check `game.log` for complete, ordered entries
6. **Score Persistence**: Verify `scores.txt` updates after each game
7. **Signal Handling**: Ctrl+C should save scores gracefully

## Requirements

- Linux/WSL with POSIX support
- GCC with pthread support
- Real-time library (`-lrt`)

## Author

Operating Systems Assignment - Demonstrating Hybrid Concurrency, IPC, and Synchronization
