CC=g++
CFLAGS=-lm `sdl2-config --cflags --libs` -lGL -lGLEW -lnoise
FASTNOISE=$(wildcard src/external/fastnoise/*.cpp)
OUTPUT=ter.out

SRC = $(wildcard src/*.cpp)

main : $(src)
	$(CC) -o $(OUTPUT) $(SRC) $(FASTNOISE) $(CFLAGS)
