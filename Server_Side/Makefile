CC = gcc
CFLAGS = -Wall -I../Client_Side
OBJS = HTTPServer.o

all: Server_Side

Server_Side: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o Server_Side -lrt

HTTPServer.o: HTTPServer.c


clean:
	rm -f *~ *.o Server_Side



