/* Story bench: flies "The Ice Run" with a human-like pilot and writes
 *   docs/narrative/play-order.json  scenes in the order they opened, per seed
 *   docs/narrative/ablation.json    story numbers on/off, one beat off, each choice path
 *   docs/narrative/tension.json     per-leg crises and failures, one row per run
 * Usage: story_bench <dir>          write all three files (make story)
 *        story_bench --check <dir>  re-run a few rows and require them verbatim in the files (make test)
 * Every run is deterministic. */
#include "voyage_run.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint64_t key; unsigned effects; int route, turned; } Job;

static RunResult run_job(Job j)
{
    RunResult r;
    run_voyage(j.key, j.effects, j.route, j.turned, &r);
    return r;
}

/* ------------------------------------------------------------------ rows */
static const char *ending_id(int e) { return e == 0 ? "end-turned" : e == 1 ? "end-held" : "none"; }

static void play_row(char *buf, size_t cap, int i, const RunResult *r)
{
    int route, turned;
    play_choices(i, &route, &turned);
    size_t n = (size_t)snprintf(buf, cap, "{\"seed\": \"%d\", \"choices\": {\"route\": %d, \"ilse\": %d}, \"order\": [", i, route, turned);
    for (int k = 0; k < r->order_len; k++) n += (size_t)snprintf(buf + n, cap - n, "%s\"%s\"", k ? ", " : "", SCENE_ID[r->order[k]]);
    n += (size_t)snprintf(buf + n, cap - n, "], \"won\": %s, \"ending\": \"%s\", \"quiet\": [", r->completed ? "true" : "false", ending_id(r->ending));
    int first = 1;
    for (int k = 0; k < r->order_len; k++) {
        int s = r->order[k];
        if (voyage_heavy(s) && r->quiet[s]) { n += (size_t)snprintf(buf + n, cap - n, "%s\"%s\"", first ? "" : ", ", SCENE_ID[s]); first = 0; }
    }
    snprintf(buf + n, cap - n, "]}");
}

static void ablation_row(char *buf, size_t cap, int i, const RunResult *r)
{
    snprintf(buf, cap, "{\"seed\": \"%d\", \"seconds\": %.2f, \"red\": %.2f}", i, r->seconds, r->red);
}

static void tension_row(char *buf, size_t cap, int i, int run, const RunResult *r)
{
    int route, turned;
    tension_choices(run, &route, &turned);
    size_t n = (size_t)snprintf(buf, cap, "{\"seed\": \"%d\", \"run\": %d, \"path\": {\"route\": \"%d\", \"ilse\": \"%d\"}, \"completed\": %s, \"crisis\": [",
                                i, run, route, turned, r->completed ? "true" : "false");
    for (int k = 0; k < V_LEGS; k++) n += (size_t)snprintf(buf + n, cap - n, "%s%d", k ? ", " : "", r->failure[k] > 0 ? r->failure[k] : r->crisis[k]);
    n += (size_t)snprintf(buf + n, cap - n, "], \"failure\": [");
    for (int k = 0; k < V_LEGS; k++) n += (size_t)snprintf(buf + n, cap - n, "%s%d", k ? ", " : "", r->failure[k]);
    snprintf(buf + n, cap - n, "]}");
}

/* ------------------------------------------------------------------ arms */
enum { ARM_ON, ARM_OFF, ARM_DOCK, ARM_BEACON, ARM_FLARE, ARM_SILENCE, ARM_NEAR, ARM_TURN, ARMS };
static const unsigned ARM_EFFECTS[ARMS] = {
    VE_ALL, 0u, VE_ALL & ~VE_DOCK, VE_ALL & ~VE_CACHE, VE_ALL & ~VE_FLARE, VE_ALL & ~VE_SILENCE, VE_ALL, VE_ALL};
static const int ARM_ROUTE[ARMS] = {DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, 0, DEFAULT_ROUTE};
static const int ARM_TURNED[ARMS] = {DEFAULT_TURNED, DEFAULT_TURNED, DEFAULT_TURNED, DEFAULT_TURNED, DEFAULT_TURNED, DEFAULT_TURNED, DEFAULT_TURNED, 0};

static Job ablation_job(int arm, int i) { return (Job){ablation_key(i), ARM_EFFECTS[arm], ARM_ROUTE[arm], ARM_TURNED[arm]}; }
static Job play_job(int i) { int r, t; play_choices(i, &r, &t); return (Job){play_key(i), VE_ALL, r, t}; }
static Job tension_job(int i, int run) { int r, t; tension_choices(run, &r, &t); return (Job){tension_key(i, run), VE_ALL, r, t}; }

/* ------------------------------------------------------------------ full output */
static FILE *open_out(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    return f;
}

static void write_rows(FILE *f, const char *indent, const RunResult *res, int arm)
{
    char row[256];
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        ablation_row(row, sizeof row, i, &res[arm * ABLATION_SEEDS + i]);
        fprintf(f, "%s%s%s\n", indent, row, i + 1 < ABLATION_SEEDS ? "," : "");
    }
}

static void write_arm_pair(FILE *f, const char *name, const RunResult *res, int off, int last)
{
    fprintf(f, "    \"%s\": {\n      \"on\": [\n", name);
    write_rows(f, "        ", res, ARM_ON);
    fprintf(f, "      ],\n      \"off\": [\n");
    write_rows(f, "        ", res, off);
    fprintf(f, "      ]\n    }%s\n", last ? "" : ",");
}

static const char *GEN = "tests/story_bench.c (make story)";
static const char *POLICY =
    "A pilot who reads the gauges, not the code: holds Raise (or Lower) until the apsis readout passes the target ring, "
    "releasing 3 % short to 8 % past it and 0-2 frames late; coasts until the tug is within 0-5 % of the ring, reacts after "
    "0-5 frames and holds Match until the match error reads below 0.004-0.02; if the orbit turns before the ring, or nothing "
    "changes for 3 T, it circularizes or burns again. On the pod's ring it cuts the engine and waits. It does not smooth the "
    "wobbling readout. Its hand comes from the run's own stirred seed, re-drawn per leg. Choices follow the path field.";

static int write_all(const char *dir)
{
    int np = PLAY_SEEDS, na = ARMS * ABLATION_SEEDS, nt = TENSION_SEEDS * TENSION_RUNS;
    RunResult *res = calloc((size_t)(np + na + nt), sizeof *res);
    if (!res) return 1;
    for (int i = 0; i < np; i++) res[i] = run_job(play_job(i));
    for (int a = 0; a < ARMS; a++)
        for (int i = 0; i < ABLATION_SEEDS; i++) res[np + a * ABLATION_SEEDS + i] = run_job(ablation_job(a, i));
    for (int i = 0; i < TENSION_SEEDS; i++)
        for (int k = 0; k < TENSION_RUNS; k++) res[np + na + i * TENSION_RUNS + k] = run_job(tension_job(i, k));
    char row[1024];

    FILE *f = open_out(dir, "play-order.json");
    fprintf(f, "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"note\": \"Scenes open at fixed progress points: the start of the run and the end of a leg. "
               "route 0 = the near buoy, 1 = the far buoy; ilse 0 = turn back for her pod, 1 = hold the heading. quiet lists the heavy scenes "
               "shown with no 'leg cleared' banner raised before the next frame of flight. Do not edit by hand.\",\n  \"seeds\": [\n", GEN);
    for (int i = 0; i < np; i++) { play_row(row, sizeof row, i, &res[i]); fprintf(f, "    %s%s\n", row, i + 1 < np ? "," : ""); }
    fprintf(f, "  ]\n}\n");
    fclose(f);

    const RunResult *ab = res + np;
    f = open_out(dir, "ablation.json");
    fprintf(f, "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"note\": \"Same %d seeds in every arm. on = all story numbers, the far buoy and holding the heading; "
               "off = scenes still open but every story number is neutral; beats = only that scene's numbers removed; choices = one choice fixed per path, "
               "the other at its default. Do not edit by hand.\",\n", GEN, ABLATION_SEEDS);
    fprintf(f, "  \"metrics\": {\n"
               "    \"seconds\": {\"better\": \"lower\", \"kind\": \"outcome\", \"felt\": \"screen\", \"label\": \"T on the mission clock at Lamp Quay; every T is flown (the pod's wait and a drifting tug included, no flat additions)\"},\n"
               "    \"red\": {\"better\": \"lower\", \"kind\": \"outcome\", \"felt\": \"screen\", \"label\": \"T flown with the tank gauge in the red (below 0.25); an empty tank stops the engine until help comes\"}\n"
               "  },\n  \"effects\": {\n    \"on\": [\n");
    write_rows(f, "      ", ab, ARM_ON);
    fprintf(f, "    ],\n    \"off\": [\n");
    write_rows(f, "      ", ab, ARM_OFF);
    fprintf(f, "    ]\n  },\n  \"beats\": {\n");
    write_arm_pair(f, "dock", ab, ARM_DOCK, 0);
    write_arm_pair(f, "beacon", ab, ARM_BEACON, 0);
    write_arm_pair(f, "flare", ab, ARM_FLARE, 0);
    write_arm_pair(f, "silence", ab, ARM_SILENCE, 1);
    fprintf(f, "  },\n  \"choices\": {\n    \"route\": {\"paths\": {\n      \"0\": [\n");
    write_rows(f, "        ", ab, ARM_NEAR);
    fprintf(f, "      ],\n      \"1\": [\n");
    write_rows(f, "        ", ab, ARM_ON);
    fprintf(f, "      ]\n    }},\n    \"ilse\": {\"paths\": {\n      \"0\": [\n");
    write_rows(f, "        ", ab, ARM_TURN);
    fprintf(f, "      ],\n      \"1\": [\n");
    write_rows(f, "        ", ab, ARM_ON);
    fprintf(f, "      ]\n    }}\n  }\n}\n");
    fclose(f);

    const RunResult *tn = res + np + na;
    f = open_out(dir, "tension.json");
    fprintf(f, "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"policy\": \"%s\",\n", GEN, POLICY);
    fprintf(f, "  \"crisis\": \"the tank gauge in the red (below 0.25) at any moment in the leg, or a failure (a failure counts as a crisis)\",\n"
               "  \"failure\": \"the tank ran dry and a drone or Lamp Quay's tug had to come, the tug fell within 0.35 L of the star or wandered "
               "45 T, or the pod's grapple glanced off\",\n"
               "  \"segments\": [\"leaving the dock\", \"the climb to the buoy\", \"down below the flare front\", \"the flare\", \"the last leg\"],\n"
               "  \"note\": \"%d seeds x %d runs; every run has its own stirred seed; the path comes from the run number.\",\n  \"seeds\": [\n",
            TENSION_SEEDS, TENSION_RUNS);
    for (int i = 0; i < nt; i++) {
        tension_row(row, sizeof row, i / TENSION_RUNS, i % TENSION_RUNS, &tn[i]);
        fprintf(f, "    %s%s\n", row, i + 1 < nt ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);

    double sec = 0;
    for (int i = 0; i < ABLATION_SEEDS; i++) sec += ab[i].seconds;
    printf("story bench: %d runs; mean run %.1f T (default path)\n", np + na + nt, sec / ABLATION_SEEDS);
    free(res);
    return 0;
}

/* ------------------------------------------------------------------ check mode */
static char *slurp(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "missing %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) exit(1);
    buf[size] = 0;
    fclose(f);
    return buf;
}

static int cmp_key(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}

static int check(const char *dir)
{
    int failures = 0;
    /* [seed] every pilot key in the bench is distinct (paired arms share a seed by design). */
    int nk = PLAY_SEEDS + ABLATION_SEEDS + TENSION_SEEDS * TENSION_RUNS, k = 0;
    uint64_t *keys = malloc(sizeof *keys * (size_t)nk);
    for (int i = 0; i < PLAY_SEEDS; i++) keys[k++] = play_key(i);
    for (int i = 0; i < ABLATION_SEEDS; i++) keys[k++] = ablation_key(i);
    for (int i = 0; i < TENSION_SEEDS; i++)
        for (int r = 0; r < TENSION_RUNS; r++) keys[k++] = tension_key(i, r);
    qsort(keys, (size_t)nk, sizeof *keys, cmp_key);
    for (int i = 1; i < nk; i++) if (keys[i] == keys[i - 1]) { fputs("[seed] duplicate run key\n", stderr); failures++; }
    free(keys);

    enum { CHECKS = 8 };
    Job jobs[CHECKS] = {play_job(0), play_job(PLAY_SEEDS - 1), ablation_job(ARM_ON, 0), ablation_job(ARM_OFF, 0),
                        ablation_job(ARM_NEAR, ABLATION_SEEDS - 1), ablation_job(ARM_TURN, ABLATION_SEEDS - 1),
                        tension_job(0, 0), tension_job(TENSION_SEEDS - 1, TENSION_RUNS - 1)};
    char *play = slurp(dir, "play-order.json"), *abl = slurp(dir, "ablation.json"), *ten = slurp(dir, "tension.json");
    char row[1024];
    /* How many times each re-run row must appear: the default arm is reused by every beat and as one path of each choice. */
    static const int copies[CHECKS] = {1, 1, 7, 1, 1, 1, 1, 1};
    for (int i = 0; i < CHECKS; i++) {
        RunResult r = run_job(jobs[i]);
        const char *hay, *file;
        if (i < 2) { play_row(row, sizeof row, i ? PLAY_SEEDS - 1 : 0, &r); hay = play; file = "play-order"; }
        else if (i < 6) { ablation_row(row, sizeof row, i < 4 ? 0 : ABLATION_SEEDS - 1, &r); hay = abl; file = "ablation"; }
        else { tension_row(row, sizeof row, i == 6 ? 0 : TENSION_SEEDS - 1, i == 6 ? 0 : TENSION_RUNS - 1, &r); hay = ten; file = "tension"; }
        int found = 0;
        for (const char *c = hay; (c = strstr(c, row)); c++) found++;
        if (found != copies[i]) {
            fprintf(stderr, "[bench] %s.json does not match the code (%d of %d copies): %s\n", file, found, copies[i], row);
            failures++;
        }
    }
    free(play); free(abl); free(ten);
    if (failures) { fprintf(stderr, "story bench check: %d failure(s); run make story to regenerate\n", failures); return 1; }
    puts("story bench check: 8 re-run rows match docs/narrative; all run seeds distinct");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--check") == 0) return check(argv[2]);
    if (argc == 2) return write_all(argv[1]);
    fputs("usage: story_bench <dir> | story_bench --check <dir>\n", stderr);
    return 2;
}
