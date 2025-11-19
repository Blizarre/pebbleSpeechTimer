.PHONY: format build dev

format:
	clang-format src/c/* -i

build:
	pebble build

dev:
	pebble build && pebble install --cloudpebble --logs
