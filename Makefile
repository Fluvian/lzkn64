# Makefile for lzkn64

CC = gcc
CFLAGS = -I. -O3
DEPS = lzkn64.h main.h types.h
OBJ = lzkn64.o main.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

lzkn64: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o lzkn64