/* $Id: dnetw.h,v 1.12 2004-09-05 20:43:22 papier Exp $ */

#ifndef __DNETW_H__
#define __DNETW_H__

/* version string */
#define GKRELLDNET_VERSION "0.14.1"

/* Crunch-o-meter style (default: 2) */
#define CRUNCH_AUTO    -1
#define CRUNCH_ABSOLUTE 1
#define CRUNCH_RELATIVE 2
#define CRUNCH_LIVERATE 3

/* a shorter type ;-) */
typedef unsigned long long int ulonglong;

#define MAX_CPU 10

/* structure to hold monitored values */
struct dnetc_values {
	char running;                /* TRUE if dnetw is running */ 
	char contest[4];             /* current contest */
	int cmode;                   /* crun-o-meter mode */
	int wu_in;                   /* work unit in input buffer */
	int wu_out;                  /* work unit in output buffer */
	int n_cpu;                   /* number of active cruncher */
	ulonglong val_cpu[MAX_CPU];  /* current cruncher value */
};

/* I need TRUE/FALSE macros */
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#endif
