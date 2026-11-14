CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
SRC     := src/orbit.c src/svg.c src/render.c src/gif.c
CR3BP   := src/cr3bp.c src/lagrange.c src/svg.c
EMCC    ?= emcc
PYTHON  ?= python3
NODE    ?= node

.PHONY: all test bench story story-check web play smoke plots clean theory-figs theory-check lagrange-plots
all: gravity-well

gravity-well: $(SRC) src/cr3bp.c src/lagrange.c src/main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_orbit: $(SRC) tests/test_orbit.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_gif: src/gif.c tests/test_gif.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_flight: src/orbit.c src/flight.c tests/test_flight.c src/flight.h src/orbit.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

VOYAGE  := src/orbit.c src/voyage.c
VOYAGE_H := src/orbit.h src/voyage.h tests/voyage_policy.h tests/voyage_run.h

test_voyage: $(VOYAGE) tests/test_voyage.c $(VOYAGE_H)
	$(CC) $(CFLAGS) -o $@ $(VOYAGE) tests/test_voyage.c $(LDLIBS)

story_bench: $(VOYAGE) tests/story_bench.c $(VOYAGE_H)
	$(CC) $(CFLAGS) -o $@ $(VOYAGE) tests/story_bench.c $(LDLIBS)

# The story rows are compared byte for byte; generate and check them with the
# Debian 13 toolchain of the Dockerfile so libm rounds the same way everywhere.
test_cr3bp: $(CR3BP) tests/test_cr3bp.c src/cr3bp.h src/lagrange.h src/orbit.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

test: test_orbit test_gif test_flight test_voyage story_bench test_cr3bp
	./test_orbit
	./test_gif
	./test_flight
	./test_voyage
	./story_bench --check docs/narrative
	./test_cr3bp

# Rewrites docs/narrative/{play-order,ablation,tension}.json.
story: story_bench
	mkdir -p docs/narrative
	./story_bench docs/narrative

# Regenerates every row into a scratch directory and compares the three files byte for byte.
story-check: story_bench
	rm -rf .story-check && mkdir .story-check
	./story_bench .story-check
	cmp .story-check/play-order.json docs/narrative/play-order.json
	cmp .story-check/ablation.json docs/narrative/ablation.json
	cmp .story-check/tension.json docs/narrative/tension.json
	rm -rf .story-check

bench: test_flight
	./test_flight --bench

web: dist/orbit.js dist/cr3bp.js
	cp web/index.html web/style.css web/app.js dist/
	cp web/lagrange.html web/lagrange.css web/lagrange.js dist/
	mkdir -p dist/art
	cp assets/art/* dist/art/
	$(PYTHON) scripts/check-size.py

dist/orbit.js: src/orbit.c src/orbit.h src/flight.c src/flight.h src/voyage.c src/voyage.h src/voyage_web.c
	mkdir -p dist
	$(EMCC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror src/orbit.c src/flight.c src/voyage.c src/voyage_web.c --no-entry -sMODULARIZE -sEXPORT_NAME=createOrbit -sENVIRONMENT=web -sFILESYSTEM=0 -sEXPORTED_FUNCTIONS='["_flight_reset","_flight_tick","_flight_value","_vw_reset","_vw_tick","_vw_answer","_vw_value"]' -o $@

dist/cr3bp.js: $(CR3BP) src/cr3bp.h src/lagrange.h src/orbit.h
	mkdir -p dist
	$(EMCC) $(CFLAGS) $(CR3BP) --no-entry -sMODULARIZE -sEXPORT_NAME=createLagrange -sENVIRONMENT=web,node -sFILESYSTEM=0 -sEXPORTED_FUNCTIONS='["_lab_reset","_lab_set_mu","_lab_tick","_lab_value","_lab_point","_lab_segment","_lab_allowed"]' -o $@

play: web
	$(PYTHON) -m http.server 4216 --bind localhost --directory dist

smoke: web
	$(NODE) scripts/smoke.cjs
	$(NODE) scripts/lagrange-smoke.cjs

# Regenerates the CSV and the SVG charts shown in the README.
plots: gravity-well
	mkdir -p docs
	./gravity-well --headless docs
	./gravity-well --gif docs/demo.gif
	head -c6 docs/demo.gif | grep -q GIF89a

# Regenerates the plotted figures and the measured-values table of docs/theory (one command).
theory_figs: $(VOYAGE) src/flight.c src/svg.c tests/theory_figs.c $(VOYAGE_H) src/flight.h src/svg.h
	$(CC) $(CFLAGS) -o $@ $(VOYAGE) src/flight.c src/svg.c tests/theory_figs.c $(LDLIBS)

theory-figs: theory_figs
	mkdir -p docs/theory/figures
	./theory_figs docs/theory/figures
	$(MAKE) lagrange-plots

lagrange-plots: gravity-well
	./gravity-well --lagrange docs/theory/figures

# Fails when a statement of docs/theory has no figure, a link is broken, or a referenced function or test is gone.
theory-check:
	$(PYTHON) scripts/theory-check.py

clean:
	rm -f gravity-well test_orbit test_gif test_flight test_voyage story_bench theory_figs test_cr3bp
	rm -rf dist
