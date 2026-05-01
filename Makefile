CC = gcc
CFLAGS = -Wall -Iinclude -Wextra -Wpedantic
LDFLAGS = -Llib -lSDL3

TARGET = image_viewer
SRC = main.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET).exe $(TARGET)
