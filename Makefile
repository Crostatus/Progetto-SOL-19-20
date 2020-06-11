CC = gcc
CFLAGS += -Wall -pedantic
objects = ./src/supermarket.o ./src/manager.o ./src/client.o ./src/cassiere.o ./src/time_tools.o
TARGET = ./bin/supermarket

.PHONY: all test clean

./bin/supermarket : $(objects)
	$(CC) -pthread -I ./headers $(CFLAGS) $^ -o $@

./src/supermarket.o : ./src/supermarket.c
	$(CC) -I ./headers -c $(CFLAGS) $^ -o $@

./src/manager.o : ./src/manager.c
	$(CC) -I ./headers -c $(CFLAGS) $^ -o $@

./src/client.o : ./src/client.c
	$(CC) -I ./headers -c $(CFLAGS) $^ -o $@

./src/cassiere.o : ./src/cassiere.c
	$(CC) -I ./headers -c $(CFLAGS) $^ -o $@

./src/time_tools.o : ./src/time_tools.c
	$(CC) -I ./headers -c $(CFLAGS) $^ -o $@

all : $(TARGET)

clean:
	@echo "Removing garbage."
	-rm -f ./bin/supermarket
	-rm -f ./log.txt
	-rm -f ./src/*.o

test :
	-rm -f ./log.txt
	./tester.sh
	./analisi.sh
