/* $Id$ */

#ifndef __DPRINT_H__
#define __DPRINT_H__

#include <stdint.h>
#include "dnetw.h"

void sprint_cpu_val(char *buf, int max, uint64_t val, struct dnetc_values *shmem);

void sprint_formated_data(char *text, int size, char *format_string, struct dnetc_values *shmem);

#endif
