PREFIX=$(HOME)/.local
USERCONF=$(HOME)/.config
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

.PHONY: install
install:
	install -m755 $(BIN_NAME) $(PREFIX)/bin/
	install -m755 ./toggle_appfinder.sh $(PREFIX)/bin/
	mkdir -p $(USERCONF)/$(BIN_NAME)
	install -m644 ./config $(USERCONF)/$(BIN_NAME)/
	install -m644 libinput-touchscreen.service $(USERCONF)/systemd/user/
