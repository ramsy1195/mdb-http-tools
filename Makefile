CC = gcc
CFLAGS = -Wall -g

all: mdb-lookup-server http-client

mdb-lookup-server: mdb-lookup-server.c
	$(CC) $(CFLAGS) -o mdb-lookup-server mdb-lookup-server.c

http-client: http-client.c
	$(CC) $(CFLAGS) -o http-client http-client.c

clean:
	rm -f mdb-lookup-server http-client
