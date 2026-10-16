CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
SRC     := src/orbit.c src/svg.c src/render.c

.PHONY: all test plots clean
all: gravity-well

gravity-well: $(SRC) src/main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_orbit: $(SRC) tests/test_orbit.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test: test_orbit
	./test_orbit

# Regenerates the CSV and the SVG charts shown in the README.
plots: gravity-well
	mkdir -p docs
	./gravity-well --headless docs

clean:
	rm -f gravity-well test_orbit
