CC=gcc
CFLAGS=-Wall -Wextra -pthread

SERVER=server
PUBLISHER=publisher
SUBSCRIBER=subscriber

all: $(SERVER) $(PUBLISHER) $(SUBSCRIBER)

$(SERVER): server.c list.c
	$(CC) $^ -o $@ $(CFLAGS)

$(PUBLISHER): publisher.c
	$(CC) $^ -o $@

$(SUBSCRIBER): subscriber.c
	$(CC) $^ -o $@ $(CFLAGS)


run: all
	gnome-terminal -- bash -c "./server; exec bash"
	sleep 1
	gnome-terminal -- bash -c "./subscriber 127.0.0.1 12345; exec bash"
	gnome-terminal -- bash -c "./subscriber 127.0.0.1 12345; exec bash"
	gnome-terminal -- bash -c "./publisher 127.0.0.1 12345; exec bash"
	gnome-terminal -- bash -c "./publisher 127.0.0.1 12345; exec bash"

clean:
	rm -f $(SERVER) $(PUBLISHER) $(SUBSCRIBER)

