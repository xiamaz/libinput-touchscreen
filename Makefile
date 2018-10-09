BIN_NAME = libinput-touchscreen

OPTS = -Wall -O2 -pipe

$(BIN_NAME): main.o list.o calibration.o configuration.o \
	libinput-backend.o libinput-touchscreen.o
	gcc -o $@ $^ -lm `pkg-config --cflags --libs libinput libudev`

%.o: src/%.c
	gcc $(OPTS) -c $^

.PHONY: clean
clean:
	rm -f ./*.o
	rm -f $(BIN_NAME)

.PHONY: run
run: $(BIN_NAME)
	./$<
