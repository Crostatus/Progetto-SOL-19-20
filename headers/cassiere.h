#ifndef cassiere_h
#define cassiere_h

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "time_tools.h"




extern void* go_work(void* desk_id);

extern void setup_market();

#endif
