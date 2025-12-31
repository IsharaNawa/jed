

all: decoder encoder decoder_c encoder_c

decoder: src/decoder.cpp
	@mkdir -p bin
	g++ --std=c++14 -O3 -o bin/decoder src/decoder.cpp

encoder: src/encoder.cpp
	@mkdir -p bin
	g++ --std=c++14 -O3 -o bin/encoder src/encoder.cpp

decoder_c: src/decoder_c.c src/jpg_c.c
	@mkdir -p bin
	gcc -std=c99 -O3 -o bin/decoder_c src/decoder_c.c src/jpg_c.c -lm

encoder_c: src/encoder_c.c src/jpg_c.c
	@mkdir -p bin
	gcc -std=c99 -O3 -o bin/encoder_c src/encoder_c.c src/jpg_c.c -lm

clean:
	rm -rf bin

help:
	@echo "Available targets: decoder, encoder, decoder_c, clean"
