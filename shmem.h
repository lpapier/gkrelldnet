/* $Id: shmem.h,v 1.4 2004-11-27 16:25:37 papier Exp $ */


#ifndef __SHMEM_H__
#define __SHMEM_H__

int my_shmcreate(int size, int shmflg);
int my_shmget(int size, int shmflg);
void my_shmdel(int shmid);

#endif
