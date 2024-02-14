xxx:
	nasm -felf64 ext.asm -o ext.o
	clang loc.c ext.o -o loc
	./loc

