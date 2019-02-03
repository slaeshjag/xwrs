all:
	$(CC) -O0 -g *.c -o xwrs.elf -lX11 -lGLX -lGL
