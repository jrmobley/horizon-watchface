
#pragma once
#include "pebble.h"

#ifdef PBL_SIMULATOR

#define SIM_LOG(fmt, args...) sim_log(fmt, ## args)

#else

#define SIM_LOG(fmt, args...)

#endif
