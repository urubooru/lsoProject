CFLAGS  = -Wall -Wextra -g -pthread
LDFLAGS = -pthread
 
all: server client

server: server.c
	$(CC) $(CFLAGS) -o server.elf server.c $(LDFLAGS)
 
client: client.c
	$(CC) $(CFLAGS) -o client.elf client.c $(LDFLAGS)
 
clean:
	rm -f server.elf client.elf
