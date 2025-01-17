CC = g++
CFLAGS = -std=c++23 -O3 -march=native -Wall

SRC1 = main.cpp
SRC2 = corpus.cpp
HDR = corpus.h
EXEC = corpus

.PHONY: all clean

all: $(EXEC)

$(EXEC): $(SRC1) $(SRC2) $(HDR)
	$(CC) $(CFLAGS) -o $(EXEC) $(SRC1) $(SRC2)

clean:
	rm -f $(EXEC)