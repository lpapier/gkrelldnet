/* $Id: shmem.h,v 1.3 2004-07-17 10:29:34 papier Exp $ */

#ifndef __SHMEM_H__
#define __SHMEM_H__

int my_shmcreate(int size, int shmflg);
int my_shmget(int size, int shmflg);
void my_shmdel(int shmid);

#endif
