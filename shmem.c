/*
|  Copyright (C) 2000-2001 Laurent Papier
|
|  Author:  Laurent Papier <papier@linuxfan.com>
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


#include <sys/ipc.h>
#include <sys/shm.h>


/*-------------------------------------------------------------
  - a wrapper to shmget and shmat
  - use a string to generate the memory segment key.
  -------------------------------------------------------------
  -*/
int my_shmget(char *text, int size, int shmflg)
{
	key_t key;

	/* generate key */
	key = (key_t) 0x1e240;
	
	/* call shmget */
	return shmget(key,size,shmflg);
}
