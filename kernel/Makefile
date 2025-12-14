all: clean build

clean:
	rm -rf build/

build:
	mkdir -p build
	cp $(CURDIR)/linker.ld build/
	c3c build -z "-Tlinker.ld"