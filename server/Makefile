
CC=$(CROSS_COMPILE)gcc
CFLAGS=-Wall -Werror
#Creating makefile with pthreads https://stackoverflow.com/questions/15367617/creating-makefile-with-pthreads
LDFLAGS = -pthread -lrt 
all: aesdsocket
	$(CC) $(CFLAGS) $(LDFLAGS) -o aesdsocket aesdsocket.c

clean:
	rm -f *.o aesdsocket
