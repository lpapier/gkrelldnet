/* a simple tool to dump the shared memory segment values
|  Copyright (C) 2000-2002 Laurent Papier
|
|  Author:  Laurent Papier <papier@tuxfan.net>
|
|  This program is free software which I release under the GNU General Public
|  License. You may redistribute and/or modify this program under the terms
|  of that license as published by the Free Software Foundation; either
|  version 2 of the License, or (at your option) any later version.
|
|  This program is distributed in the hope that it will be useful,
|  but WITHOUT ANY WARRANTY; without even the implied warranty of
|  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
|  GNU General Public License for more details.
| 
|  To get a copy of the GNU General Puplic License, write to the Free Software
|  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "shmem.h"
#include "dnetw.h"

int main(int argc,char *argv[])
{
	struct dnetc_values *shmem;          /* values from the wrapper */
	int shmid,i;
	float tmp;

	if((shmid = my_shmget(sizeof(struct dnetc_values),0660)) == -1)
	{
		printf("no shared memory segment\n");
		exit(-1);
	}
	
	if((int) (shmem = shmat(shmid,0,0)) == -1) {
		printf("can't attach shared memory segment\n");
		exit(-1);
	}

	if(!shmem->running) {
		printf("dnetw: not running\n");
	}
	else {
		printf("dnetw: running\n");
		printf(" -> working on %s\n",shmem->contest);
		printf(" -> work unit in buffers %d/%d\n",shmem->wu_in,shmem->wu_out);
		printf(" -> %d active CPU\n",shmem->n_cpu);
		switch(shmem->cmode)
		{
			case CRUNCH_RELATIVE:
				for(i=0;i<shmem->n_cpu;i++)
					printf(" -> %llu%% done on CPU%d\n",shmem->val_cpu[i],i);
				break;
			case CRUNCH_ABSOLUTE:
				for(i=0;i<shmem->n_cpu;i++) {
					if(!strcmp(shmem->contest,"OGR"))
					{
						/* do auto-scale */
						tmp = (float) (shmem->val_cpu[i] / 1000000ULL);
						printf(" -> %.2f Gn done on CPU%d\n",tmp/1000,i);
					}
					if(!strcmp(shmem->contest,"RC5"))
					{
						/* do auto-scale */
						tmp = (float) (shmem->val_cpu[i] / 1000ULL);
						printf(" -> %.2f Mk done on CPU%d\n",tmp/1000,i);
					}
					
				}
				break;
		}
	}

	shmdt(shmem);
	exit(0);
}

