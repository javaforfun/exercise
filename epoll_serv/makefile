CC=gcc
INC+= 
LIB+=-lpthread
CFLAGS+=-g -Wall -D_GNU_SOURCE
TARGET=serv

all:$(TARGET)

serv: serv.o worker.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIB)

%.o: %.c
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	rm -f *.o *~ *#
	rm -f $(TARGET)

love: clean all

