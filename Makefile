all: genext2fs

genext2fs: genext2fs.c
	gcc -O2 -Wall -o $@ $<

