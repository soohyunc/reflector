#ifndef __UI_CONTROLLER__
#define __UI_CONTROLLER__
#include "queue.h"

int init_ui();
int channels_to_ui(struct queue_s *c);
int process_ui();

#endif
