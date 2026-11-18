# M61: registers the existing checks as one CTest testcase each, so JUnit counts real programs.
# Labels select the kind: unit (what `make test` runs), trace (the [REQ-...] subset), perf, e2e.
# Generated JUnit: `make test-tm-<kind>` -> test-results/<kind>.xml. Run through `make check`.
cmake_minimum_required(VERSION 3.20)
project(gravity_well_checks NONE)
enable_testing()
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
find_program(TM_MAKE make REQUIRED)
find_program(TM_PYTHON NAMES python3.14 python3 REQUIRED)

function(tm_check name labels)
  add_test(NAME "${name}" COMMAND ${ARGN})
  set_tests_properties("${name}" PROPERTIES WORKING_DIRECTORY "${ROOT}" LABELS "${labels}")
endfunction()

tm_check("test_orbit [REQ-orbit-physics]" "unit;trace" ./test_orbit)
tm_check("test_gif" "unit" ./test_gif)
tm_check("test_flight [REQ-orbit-physics]" "unit;trace" ./test_flight)
tm_check("test_voyage [REQ-ice-run]" "unit;trace" ./test_voyage)
tm_check("story_bench --check [REQ-ice-run]" "unit;trace" ./story_bench --check docs/narrative)
tm_check("test_cr3bp [REQ-rotating-frame]" "unit;trace" ./test_cr3bp)
tm_check("theory-check" "unit" ${TM_PYTHON} scripts/theory-check.py)
tm_check("bench" "perf" ${TM_MAKE} -j1 bench)
tm_check("smoke" "e2e" ${TM_MAKE} -j1 smoke)
