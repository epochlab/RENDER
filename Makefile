.PHONY: all run clean

all:
	cmake --preset default
	cmake --build --preset default -j

run: all
	./build/KODAK

clean:
	rm -rf build
