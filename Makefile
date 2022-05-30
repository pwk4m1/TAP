
cc=gcc
cflags=-O2 -lpthread -I./include
name=tap

all: clean build

clean:
	rm -rf bin/$(name)

libyaml:
	cd yaml-0.2.5
	./configure
	make
	make install
	cd ..

build:
	$(cc) $(cflags) -o bin/$(name) src/*.c

test:
	./bin/tap


