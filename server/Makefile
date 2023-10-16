
CC=$(CROSS_COMPILE)gcc
CFLAGS=-Wall -Werror -g
#Creating makefile with pthreads https://stackoverflow.com/questions/15367617/creating-makefile-with-pthreads
LDFLAGS = -pthread -lrt
TARGET = aesdsocket
SRC = aesdsocket.c
all: 
	$(CC) $(CFLAGS) $(LDFLAGS) -o  $(TARGET) $(SRC) 

clean:
	rm -f *.o *.elf *.map *.txt $(TARGET)
