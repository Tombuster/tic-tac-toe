CC=gcc
CFLAGS=-I. -D_GNU_SOURCE -Wall -Wextra
SRC=src
DST=.

all: $(DST)/server $(DST)/client

$(DST)/server: $(SRC)/server.c $(SRC)/db.c | $(DST)
	@echo "Compiling server"
	$(CC) $(CFLAGS) -o $@ $(SRC)/server.c $(SRC)/db.c -lsctp -lpthread

$(DST)/client: $(SRC)/client.c | $(DST)
	@echo "Compiling client"
	$(CC) $(CFLAGS) -o $@ $(SRC)/client.c -lsctp

$(DST):
	mkdir -p $(DST)

clean:
	rm -f $(DST)/server $(DST)/client