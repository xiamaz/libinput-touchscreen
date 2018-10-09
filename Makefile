main: main.c
	gcc -o $@ $< -lm `pkg-config --cflags --libs libinput libudev`

.PHONY: run
run: main
	./$<
