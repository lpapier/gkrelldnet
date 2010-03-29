/* $Id$ */

/*
|  Copyright (C) 2010-2010 Laurent Papier
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
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "shmem.h"
#include "dnetw.h"

/* format cpu val with suffix depending on crunch-o-meter mode */
void sprint_cpu_val(char *buf, int max, uint64_t val, struct dnetc_values *shmem)
{
    float tmp;

    /* add suffix depending on crunch-o-meter mode */
    switch(shmem->cmode)
    {
        case CRUNCH_RELATIVE:
            snprintf(buf,max,"%llu%%",val);
            break;
        case CRUNCH_ABSOLUTE:
            if(!strcmp(shmem->contest,"OGR"))
            {
                /* do auto-scale */
                tmp = (float) (val / 1000000ULL);
                snprintf(buf,max,"%.2fGn",tmp/1000);
            }
            if(!strcmp(shmem->contest,"RC5"))
            {
                /* do auto-scale */
                tmp = (float) (val / 1000ULL);
                snprintf(buf,max,"%.2fMk",tmp/1000);
            }

            break;
    }
}

/* fill text with current crunch-o-meter data respecting format_string */
void sprint_formated_data(char *text, int size, char *format_string, struct dnetc_values *shmem)
{
    char *s,*p,buf[24];
    int t;

        text[0] = '\0';
        if(shmem == NULL || shmem->contest[0] == '?')
        {
            snprintf(text,127,"dnet");
            return;
        }

        for(s=format_string; *s!='\0'; s++)
        {
            buf[0] = *s;
            buf[1] = '\0';
            if(*s == '$' && *(s+1) != '\0')
            {
                switch(*(s+1))
                {
                    case 'i':
                        snprintf(buf,12,"%d",shmem->wu_in);
                        s++;
                        break;
                    case 'o':
                        snprintf(buf,12,"%d",shmem->wu_out);
                        s++;
                        break;
                    case 'p':
                        t = *(s+2) - '0';
                        /* support up to 10 CPU */
                        if(t >= 0 && t <= 9)
                        {
                            if(t < shmem->n_cpu)
                                sprint_cpu_val(buf,24,shmem->val_cpu[t],shmem);
                            s++;
                        }
                        else
                            sprint_cpu_val(buf,24,shmem->val_cpu[0],shmem);
                        s++;
                        break;
                    case 'c':
                        snprintf(buf,12,"%s",shmem->contest);
                        for(p=buf;*p!='\0';p++)
							*p = tolower(*p);
                        s++;
                        break;
                    case 'C':
                        snprintf(buf,12,"%s",shmem->contest);
                        for(p=buf;*p!='\0';p++)
							*p = toupper(*p);
                        s++;
                        break;
                }
            }
            if(strlen(text) + strlen(buf) + 1 < size)
				strcat(text,buf);
			else
				return;
        }
}
