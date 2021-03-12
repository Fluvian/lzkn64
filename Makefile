# Makefile for LZKN64

lzkn64:
	gcc -o lzkn64 lzkn64.c -I .

clean:
	rm lzkn64