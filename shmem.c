/* $Id: shmem.c,v 1.7 2004-09-05 20:43:22 papier Exp $ */

/*
|  Copyright (C) 2000-2003 Laurent Papier
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>


/* filename with shared memory key */
#define SHMKEY_FILE "/tmp/dnetw-shmid"

/*-------------------------------------------------------------
  - a wrapper to shmget and shmat. Create a new shared memory segment
  - and write the key in a file.
  -------------------------------------------------------------
  -*/
int my_shmcreate(int size, int shmflg)
{
	int i,fd,shmid;
	key_t key;

	/* open file SHMKEY_FILE for writing */
	fd = open(SHMKEY_FILE,O_WRONLY|O_CREAT|O_EXCL,0644);
	if(fd == -1) return -1;

	/* look for a valid key */
	key = (key_t) 0x16fc452; i = 0;
	while((shmid = shmget(key,size,shmflg)) == -1 && i < 20) {
		i++; key += 6*i;
	}

	/* write the key in file */
	if(shmid != -1)
		write(fd,&key,sizeof(key_t));
	close(fd);

	/* return shmid */
	return shmid;
}

/*-------------------------------------------------------------
  - a wrapper to shmget and shmat. Try to open a already existing
  - shared memory segment. Read the key in a file.
  -------------------------------------------------------------
  -*/
int my_shmget(int size, int shmflg)
{
	int fd,n;
	key_t key;

	/* read the key from SHMKEY_FILE */
	fd = open(SHMKEY_FILE,O_RDONLY);
	if(fd == -1) return -1;
	n = read(fd,&key,sizeof(key_t));
	close(fd);

	/* call shmget */
	if(n != -1)
		return shmget(key,size,shmflg);
	else
		return -1;
}

/*--------------------------------------------------
  - Remove the file SHMKEY_FILE
  --------------------------------------------------
  -*/
void my_shmdel(int shmid)
{
	shmctl(shmid, IPC_RMID, 0);
	unlink(SHMKEY_FILE);
}
