
#ifndef __GKRELLDNET_H__
#define __GKRELLDNET_H__

#include <gkrellm/gkrellm.h>

#define	CONFIG_NAME	"Distributed.net"  /* Name in the configuration window */
#define CONFIG_KEYWORD "dnet"
#define	STYLE_NAME	"dnet"		  /* Theme subdirectory name and gkrellmrc */

#ifdef GRAVITY
#define MY_PLACEMENT (MON_CPU | GRAVITY(12))
#else
#define MY_PLACEMENT (MON_CPU)
#endif

#include "dnetw.h"

#endif
