#include "voyage.h"

/* Browser bridge for "The Ice Run": the same C voyage the story bench flies. */
static Voyage voyage;

void vw_reset(void) { voyage_init(&voyage, VE_ALL); }
static int command;

void vw_command(int input) { command = input; }
void vw_tick(int frames) { voyage_tick(&voyage, (VoyageInput){frames, command}); }
int vw_answer(int option) { return voyage_answer(&voyage, option); }
double vw_value(int field) { return voyage_value(&voyage, field); }
