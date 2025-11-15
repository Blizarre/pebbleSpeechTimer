.PHONY: format build

format:
	clang-format src/c/* -i

build:
	pebble build
