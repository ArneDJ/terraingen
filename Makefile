CC=g++ -std=c++14
CFLAGS=-lm `sdl2-config --cflags --libs` -lGL -lGLEW -lnoise
FASTNOISE=$(wildcard src/external/fastnoise/*.cpp)
IMGUI=$(wildcard src/external/imgui/*.cpp)
HEMAN=lib/libheman.a
OUTPUT=ter.out

SRC = $(wildcard src/*.cpp)

main : $(src)
	$(CC) -o $(OUTPUT) $(SRC) $(FASTNOISE) $(IMGUI) $(HEMAN) $(CFLAGS) 
