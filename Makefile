CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -O2
LDFLAGS = -lrt -lpthread

# Server components
SERVER_OBJS = server.o game_state.o logger.o scheduler.o sync.o
SERVER_TARGET = monopoly_server

# Client components  
CLIENT_OBJS = client.o board.o game_logic.o
CLIENT_TARGET = monopoly_client

# All targets
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# Build server
$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build client
$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies
server.o: server.c game_state.h logger.h scheduler.h
game_state.o: game_state.c game_state.h logger.h
logger.o: logger.c logger.h
scheduler.o: scheduler.c scheduler.h sync.h logger.h
sync.o: sync.c sync.h
shared_memory.o: shared_memory.c shared_memory.h
client.o: client.c player.h board.h game_logic.h
board.o: board.c board.h
game_logic.o: game_logic.c game_logic.h player.h board.h

# Clean build artifacts
clean:
	rm -f *.o $(SERVER_TARGET) $(CLIENT_TARGET)
	rm -f game.log scores.txt
	rm -f /dev/shm/monopoly_*
	rm -f /dev/mqueue/monopoly_*

# Clean and rebuild
rebuild: clean all

.PHONY: all clean rebuild
