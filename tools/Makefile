.PHONY: all build clean

all: build

build:
	mkdir -p "build/"
	g++ ndrcreator/main.cpp -o build/ndrcreator -std=c++17 -lz
	g++ entryreader/main.cpp -o build/entryreader -std=c++17
	g++ nffcreator/main.cpp -o build/nffcreator -std=c++17

clean:
	rm -rf "build/"