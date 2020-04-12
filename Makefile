CC=g++
CFLAGS=-lm `sdl2-config --cflags --libs` -lGL -lGLEW
OUTPUT=ter.out

SRC = $(wildcard src/*.cpp)

main : $(src)
	$(CC) -o $(OUTPUT) $(SRC) $(CFLAGS)
