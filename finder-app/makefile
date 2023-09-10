# File Description: makefile for Assignment 2
# Author: Visweshwaran Baskaran
# Date: 09-10-2023
# References:
#	[1]  	https://stackoverflow.com/questions/70824666/how-to-cross-compile-for-arm-on-ubuntu-conditionally#conditionally

CC=$(CROSS_COMPILE)gcc
CFLAGS= -Wall -Werror

all: writer
writer: writer.o
	$(CC) $(CFLAGS) writer.c -o writer
	
clean:
	rm *.o writer
