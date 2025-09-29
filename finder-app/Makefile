CC=$(CROSS_COMPILE)gcc

.DEFAULT_GOAL := writer
writer:
	$(CC) -o writer writer.c
	chmod +x writer

all:
	$(CC) -o writer writer.c
	chmod +x writer

clean:
	rm -rf writer
