CC=$(CROSS_COMPILE)gcc

.DEFAULT_GOAL := all
all:
	$(CC) -o aesdsocket aesdsocket.c
	chmod +x aesdsocket

clean:
	rm -rf aesdsocket

distclean: clean
