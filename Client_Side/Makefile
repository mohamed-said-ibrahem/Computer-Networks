CC = gcc
CFLAGS = -Wall 
OBJS = HTTPClient.o

all: Client_Side

Client_Side: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o Client_Side 

HTTPClient.o: HTTPClient.c


clean:
	rm -f *~ *.o Client_Side


