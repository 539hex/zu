all: zu.c
	gcc -O2 -Wall -W zu.c -o zu -g -ggdb

clean: 
	rm -f zu