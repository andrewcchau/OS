# generic copy from interwebs of a Makefile

# compiler: gcc for C
CC = gcc


# compiler flags
# -g      debugging info to executable
# -Wall   turns on most compiler warnings (not used this time)
CFLAGS = -g

# build target executable
TARGET = yash


all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	$(RM) $(TARGET)