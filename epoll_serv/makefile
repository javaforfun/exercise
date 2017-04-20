C=gcc
CC=g++
INC+= 
LIB+=-lpthread
CFLAGS+=-g -O2 -Wall -D_GNU_SOURCE
TARGET=serv

all:$(TARGET)

serv: serv.o worker.o
	$(C) $(CFLAGS) -o $@ $^ $(LIB)

%.o: %.c
	$(C) $(CFLAGS) $(INC) -c -o $@ $<

%.o: %.cc
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	rm -f *.o *~ *#
	rm -f $(TARGET)

love: clean all
