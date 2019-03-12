TARGET=chat
OBJECTS=$(BUILD)/chat.o
LIBS=
ARGS=-O3 -Wall
CLEAN=rm -f
CC=gcc
SRC=./src
BUILD=./build

$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(ARGS) $(OBJECTS) $(LIBS)

$(BUILD)/chat.o: $(SRC)/chat.c
	$(CC) -c -o $(BUILD)/chat.o $(ARGS) $(SRC)/chat.c

clean:
	$(CLEAN) $(OBJECTS)

cleanall:
	$(CLEAN) $(OBJECTS) $(TARGET)
