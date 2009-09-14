/* ------------------------------------------------------------------------- */
/* (C) 1996-2000 Orion Hodson.                                               */

#ifndef __UI_CONTROLLER__
#define __UI_CONTROLLER__
#include "queue.h"

int ui_init(struct queue_s *channels);
int ui_process();
void ui_update();

#endif
