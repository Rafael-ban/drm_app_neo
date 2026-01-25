#pragma once
#include <utils/uuid.h>
#include "apps/apps_types.h"

int apps_init(apps_t *apps,bool use_sd);
int apps_destroy(apps_t *apps);

int apps_try_launch_by_file(apps_t *apps,const char* working_dir,const char *basename);