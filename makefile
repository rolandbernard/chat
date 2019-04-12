TARGET=chat
OBJECTS=$(BUILD)/main.o $(BUILD)/cipher.o $(BUILD)/client.o $(BUILD)/hash.o $(BUILD)/image.o\
		$(BUILD)/netio.o $(BUILD)/random.o $(BUILD)/server.o $(BUILD)/termio.o
LIBS=-lm
ARGS=-g -Wall
CLEAN=rm -f
CC=gcc
SRC=./src
BUILD=./build

$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(ARGS) $(OBJECTS) $(LIBS)

$(BUILD)/main.o: $(SRC)/main.c $(SRC)/server.h $(SRC)/client.h $(SRC)/types.h
	$(CC) -c -o $(BUILD)/main.o $(ARGS) $(SRC)/main.c

$(BUILD)/netio.o: $(SRC)/netio.c $(SRC)/netio.h $(SRC)/cipher.h $(SRC)/hash.h $(SRC)/types.h
	$(CC) -c -o $(BUILD)/netio.o $(ARGS) $(SRC)/netio.c

$(BUILD)/cipher.o: $(SRC)/cipher.c $(SRC)/cipher.h $(SRC)/hash.h $(SRC)/types.h
	$(CC) -c -o $(BUILD)/cipher.o $(ARGS) $(SRC)/cipher.c

$(BUILD)/random.o: $(SRC)/random.c $(SRC)/random.h $(SRC)/hash.h $(SRC)/types.h
	$(CC) -c -o $(BUILD)/random.o $(ARGS) $(SRC)/random.c

$(BUILD)/client.o: $(SRC)/client.c $(SRC)/client.h $(SRC)/termio.h $(SRC)/netio.h $(SRC)/random.h $(SRC)/hash.h $(SRC)/types.h
	$(CC) -c -o $(BUILD)/client.o $(ARGS) $(SRC)/client.c

$(BUILD)/server.o: $(SRC)/server.c $(SRC)/server.h $(SRC)/types.h
	$(CC) -c -o $(BUILD)/server.o $(ARGS) $(SRC)/server.c

$(BUILD)/hash.o: $(SRC)/hash.c $(SRC)/hash.h $(SRC)/types.h
	$(CC) -c -o $(BUILD)/hash.o $(ARGS) $(SRC)/hash.c

$(BUILD)/termio.o: $(SRC)/termio.c $(SRC)/termio.h $(SRC)/types.h
	$(CC) -c -o $(BUILD)/termio.o $(ARGS) $(SRC)/termio.c

$(BUILD)/image.o: $(SRC)/image.c $(SRC)/image.h
	$(CC) -c -o $(BUILD)/image.o $(ARGS) $(SRC)/image.c

clean:
	$(CLEAN) $(OBJECTS)

cleanall:
	$(CLEAN) $(OBJECTS) $(TARGET)
