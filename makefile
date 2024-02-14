loc: ext.asm loc.c makefile
	nasm -felf64 ext.asm -o ext.o
	gcc loc.c ext.o -o loc

run: loc
	./loc
