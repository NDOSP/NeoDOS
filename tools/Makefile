.PHONY: all build clean

all: clean build

build:
	mkdir -p "build/"
	g++ ndrcreator/main.cpp -o build/ndrcreator -std=c++17 -lz

clean:
	rm -rf "build/"