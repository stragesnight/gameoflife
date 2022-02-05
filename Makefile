CC=gcc
CFLAGS=-O3
LIBS=-lpthread
TARGET=gameoflife

.PHONY: all clean

all:
	$(CC) main.c $(CFLAGS) $(LIBS) -o $(TARGET)

run: all
	./$(TARGET)

speedtest:
	$(CC) main.c $(CFLAGS) -D SPEED_TEST $(LIBS) -o $(TARGET)
	bash -c "time ./"$(TARGET)

clean:
	rm -f $(TARGET)

