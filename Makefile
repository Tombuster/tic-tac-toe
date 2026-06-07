CC=gcc
CFLAGS=-I.
SRC=src
DST=bin
#CFLAGS=                                                                        

OBJECTS = server client


all: $(OBJECTS)

$(OBJECTS):%:$(SRC)/%.c
	@echo Compiling $<  to  $@
	$(CC) -o $@ $< $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm  $(OBJECTS)