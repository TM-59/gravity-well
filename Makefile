CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
SRC     := src/orbit.c src/svg.c src/render.c src/gif.c
EMCC    ?= emcc
PYTHON  ?= python3
NODE    ?= node

.PHONY: all test bench web play smoke plots clean
all: gravity-well

gravity-well: $(SRC) src/main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_orbit: $(SRC) tests/test_orbit.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_gif: src/gif.c tests/test_gif.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_flight: src/orbit.c src/flight.c tests/test_flight.c src/flight.h src/orbit.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

test: test_orbit test_gif test_flight
	./test_orbit
	./test_gif
	./test_flight

bench: test_flight
	./test_flight --bench

web: dist/orbit.js
	cp web/index.html web/style.css web/app.js dist/
	mkdir -p dist/art
	cp assets/art/* dist/art/
	$(PYTHON) scripts/check-size.py

dist/orbit.js: src/orbit.c src/orbit.h src/flight.c src/flight.h
	mkdir -p dist
	$(EMCC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror src/orbit.c src/flight.c --no-entry -sMODULARIZE -sEXPORT_NAME=createOrbit -sENVIRONMENT=web -sFILESYSTEM=0 -sEXPORTED_FUNCTIONS='["_flight_reset","_flight_tick","_flight_value"]' -o $@

play: web
	$(PYTHON) -m http.server 4216 --bind localhost --directory dist

smoke: web
	$(NODE) scripts/smoke.cjs

# Regenerates the CSV and the SVG charts shown in the README.
plots: gravity-well
	mkdir -p docs
	./gravity-well --headless docs
	./gravity-well --gif docs/demo.gif
	head -c6 docs/demo.gif | grep -q GIF89a

clean:
	rm -f gravity-well test_orbit test_gif test_flight
	rm -rf dist
