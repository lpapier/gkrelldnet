/* $Id: dnetw.c,v 1.25 2004-11-28 10:52:38 papier Exp $ */

/* dnetw: a distributed.net client wrapper
|  Copyright (C) 2000-2003 Laurent Papier
|
|  Author:  Laurent Papier    papier@tuxfan.net
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
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <regex.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* openpty include file(s) */
#if defined(__linux__)
#include <pty.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <libutil.h>
#endif

#include "shmem.h"
#include "dnetw.h"

#define BUF_SIZE 256
#define MASTER_PTY "/dev/ptmx"

static int dflag = 0, qflag = 0;


/* pid of the daemon */
static pid_t new_pid;
/* pid of the dnet client */
static 	pid_t fils;
static	char **nargv = NULL;
static  int nargc;
/* tty file descriptor */
static int fd = -1, tty_fd = -1;
/* monitor file fd */
static int shmid;
static struct dnetc_values *shmem = NULL;
/* regex */
static int regex_flag = 0;
static regex_t preg_in, preg_out, preg_contest, preg_cruncher;
static regex_t preg_proxy, preg_absolute;


/* clean exit function */
static void clean_and_exit(char *text,int r)
{
	int i;

	if(text != NULL)
		perror(text);
	if(nargv != NULL)
	{
		for(i=0;i<nargc;i++)
			free(nargv[i]);
		free(nargv);
	}
	if(regex_flag)
	{
		regfree(&preg_in); regfree(&preg_out);
		regfree(&preg_contest); regfree(&preg_cruncher);
		regfree(&preg_proxy); regfree(&preg_absolute);
	}
	if((int) shmem != -1 && shmem != NULL)
	{
		shmem->running = FALSE;
		my_shmdel(shmid);
		shmdt(shmem);
	}
	if(tty_fd != -1)
		close(tty_fd);
	if(fd != -1)
		close(fd);
	exit(r);
}

/* signal handler */
static void got_signal(int signum)
{
	char buf[BUF_SIZE];
	int lu;

	/* child quit => dnet client quit */
	if(signum == SIGCHLD)
	{
		wait(NULL);
		clean_and_exit(NULL,0);
	}
	/* else we stop dnet client */
	if(kill(fils,SIGQUIT) == -1)
		clean_and_exit("kill",1);

	if(!qflag)
	{
		while((lu = read(fd,buf,BUF_SIZE)) > 0)
		{
			buf[lu] = '\0';
			printf("%s",buf);
		}
	}
	clean_and_exit(NULL,0);
}

/* change to dnetc directory: dnetc must be in the path */
static void change_dir(void)
{
	char path[1024],*tmp,file[512];
	struct stat st;

	/* get path */
	strcpy(path,getenv("PATH"));
	if(path != NULL)
	{
		/* first token */
		tmp = strtok(path,":");
		while(tmp != NULL)
		{
			strcpy(file,tmp);
			strcat(file,"/dnetc");
			/* file exist ? */
			if(stat(file,&st) != -1 && S_ISREG(st.st_mode)
			   && (st.st_mode & S_IXUSR))
			{
				/* dir. found */
				if(chdir(tmp) == 0)
					return;
			}
			/* next token */
			tmp = strtok(NULL,":");
		}
	}
}

/* extract keys or nodes from dnetc output */
static ulonglong extract_val(char *buf)
{
	char line[BUF_SIZE],t[24],*tmp;
	ulonglong val = 0;
	int i,j;

	/* make a local copy of line */
	strcpy(line,buf);

	/* search for first '[' char */
	tmp = strtok(line,"[");
	if(tmp != NULL)
	{
		tmp = strtok(NULL,"]");
		if(tmp != NULL)
		{
			for(i=0,j=0;i<strlen(tmp);i++)
				if(tmp[i] != ',')
					t[j++] = tmp[i];
			t[j] = '\0';
			sscanf(t,"%llu",&val);
			if(dflag)
				printf("-> val = %llu\n",val);
		}
	}

	return val;
}

/* read arg for '-c' option */
int get_arg(char ***args, char *str)
{
	char *tmp;
	int i,n = 0;

	/* allocate some pointers */
	for(i=0;i<strlen(str);i++)
	{
		if(str[i] == ' ')
			n++;
	}
	if((*args = (char **) calloc(n+2,sizeof(char *))) == NULL)
		clean_and_exit("calloc",1);

	/* first token */
	i = 0;
	tmp = strtok(str," ");
	while(tmp != NULL)
	{
		/* allocate some RAM */
		if(((*args)[i] = (char *) calloc(strlen(tmp),sizeof(char))) == NULL)
			clean_and_exit("calloc",1);

		strcpy((*args)[i],tmp);

		/* next token */
		tmp = strtok(NULL," ");
		i++;
	}

	/* last pointer should be NULL */
	(*args)[i] = NULL;
	return i;
}

/* a fgets for int fd */
int mygets(int fd,char *buf,int count)
{
	static char tmp[2*BUF_SIZE] = "";
	char *t = NULL;
	int s,p;

	strcpy(buf,tmp);
	p = strlen(buf);
	t = strchr(buf,'\n');

	if(t == NULL)
	{
		do
		{
			s = read(fd,&buf[p],count-p);
			p += s;
			buf[p] = '\0';
			t = strchr(buf,'\n');
		}
		while(s > 0 && t == NULL && buf[0] != 0x0d
			  && buf[0] != '/' && buf[0] != '|'
			  && buf[0] != '-' && buf[0] != '\\');
	}

	if(t != NULL)
	{
		strcpy(tmp,t+1);
		p = (t-buf+1);
		buf[p] = '\0';
	}
	else
		tmp[0] = '\0';

	return p;
}

static void usage(char *pname)
{
	fprintf(stderr,"Distributed.net client wrapper v%s\n",GKRELLDNET_VERSION);
	fprintf(stderr,"usage: %s [-q] [-o] [-l<file>] [-c<cmd>]\n",pname);
	fprintf(stderr," -q: disable all terminal output and run in background\n");
	fprintf(stderr," -o: old log format (dnetc v2.8010 and lower)\n");
	fprintf(stderr," -l<log_file>: redirect the client output to <file>\n");
	fprintf(stderr," -c<cmd>: use <cmd> to start the dnetc client (default: 'dnetc')\n");
	clean_and_exit(NULL,0);
}

int main(int argc,char *argv[])
{
	extern char *optarg;
	extern int optind;
	int ch;

	int oflag = 0;
	int contest_offset;

	char *tmp;
	char buf[BUF_SIZE];
	int ttylog,lu,i,q;

	regmatch_t pmatch[2];

	char defcmd[] = "dnetc", logfile[128] = "stdout";
	int pos_cpu[MAX_CPU];
	char p[] = "a";
	int log_fd;

	/* check arguments */
	while ((ch = getopt(argc, argv, "hdoql:c:")) != -1)
	{
		switch(ch)
		{
			case 'd':
				dflag = 1;
				break;
			case 'o':
				oflag = 1;
				break;
			case 'q':
				qflag = 1;
				break;
			case 'c':
				nargc = get_arg(&nargv,optarg);
				break;
			case 'l':
				strcpy(logfile,optarg);
				break;
			default:
				usage(argv[0]);
		}
	}
	if(argc - optind != 0)
		usage(argv[0]);

	/* continue in background if in quiet mode */
	if(qflag == 1 && ((new_pid = fork()) != 0))
	{
		if(new_pid < 0)
			clean_and_exit("forking daemon",1);
		else
			exit(0);
	}

	/* default command line */
	if(nargv == NULL)
		nargc = get_arg(&nargv,defcmd);

	/* change output to logfile */
	if(strcmp(logfile,"stdout") != 0)
	{
		if((log_fd = open(logfile, O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1)
			clean_and_exit("opening logfile",1);
		if(dup2(log_fd,1) == -1)
			clean_and_exit("dup2",1);
	}
	ttylog = isatty(1);

	/* creat shared memory segment */
	if((shmid = my_shmcreate(sizeof(struct dnetc_values),IPC_CREAT|IPC_EXCL|0644)) == -1)
		clean_and_exit("shmget",1);
	if((int) (shmem = shmat(shmid,0,0)) == -1)
		clean_and_exit("shmat",1);

	/* init shared memory content */
	shmem->running = TRUE;
	strcpy(shmem->contest,"???");
	shmem->cmode = CRUNCH_RELATIVE;
	shmem->wu_in = shmem->wu_out = 0;
	shmem->n_cpu = 1;
	for(i=0;i<MAX_CPU;i++)
		shmem->val_cpu[i] = 0;
	
	/* precompile regex */
	if(oflag)
	{
		if(regcomp(&preg_in,"[0-9]+.work.unit.*buff-in",REG_EXTENDED) != 0)
			clean_and_exit(NULL,1);
		if(regcomp(&preg_out,"[0-9]+.work.unit.*buff-out",REG_EXTENDED) != 0)
			clean_and_exit(NULL,1);
		if(regcomp(&preg_contest,"Loaded.[A-Z0-9]{3}.",REG_EXTENDED) != 0)
			clean_and_exit(NULL,1);
		if(regcomp(&preg_proxy,"(Retrieved|Sent).+(work.unit|packet)",REG_EXTENDED) !=0)
			clean_and_exit(NULL,1);

		contest_offset = 7;

		/* force relative crunch-o-meter style */
		shmem->cmode = CRUNCH_RELATIVE;
	}
	else
	{
		if(regcomp(&preg_in,"[0-9]+.packets?.+remains?.in",REG_EXTENDED) != 0)
			clean_and_exit(NULL,1);
		if(regcomp(&preg_out,"[0-9]+.packets?(.+in.buff-out|.\\(.+stats?.units?\\).are.in)",REG_EXTENDED) != 0)
			clean_and_exit(NULL,1);
		if(regcomp(&preg_contest,"[A-Z0-9-]{3,6}:.Loaded",REG_EXTENDED) != 0)
			clean_and_exit(NULL,1);
		if(regcomp(&preg_proxy,"((Retrieved|Sent).+(stat..unit|packet)|Attempting.to.resolve|Connect(ing|ed).to)",REG_EXTENDED) !=0)
			clean_and_exit(NULL,1);

		contest_offset = 0;
	}

	if(regcomp(&preg_absolute,"#[0-9]+: [A-Z0-9-]{3,6}:.+\\[[,0-9]+\\]",REG_EXTENDED) != 0)
		clean_and_exit(NULL,1);
	if(regcomp(&preg_cruncher,"[0-9]+.cruncher.*started",REG_EXTENDED) != 0)
		clean_and_exit(NULL,1);
	regex_flag = 1;

	/* obtain a pseudo-terminal */
	if((openpty(&fd,&tty_fd,NULL,NULL,NULL)) == -1)
		clean_and_exit("openpty",1);

	/* start dnet client and start reading tty */
	if((fils = fork()) == -1)
		clean_and_exit("fork",1);

	if(fils == 0)
	{
		/* change to dnetc directory */
		change_dir();
		/* start dnet client */
		if(dup2(tty_fd,1) == -1)
			clean_and_exit("dup2",1);
		if(execvp(nargv[0],nargv) == -1)
			clean_and_exit("execvp",1);
	}

	/* set signal handler */
	if(signal(SIGHUP,got_signal) == SIG_ERR
	   || signal(SIGINT,got_signal) == SIG_ERR
	   || signal(SIGQUIT,got_signal) == SIG_ERR
	   || signal(SIGCHLD,got_signal) == SIG_ERR
	   || signal(SIGTERM,got_signal) == SIG_ERR)
		clean_and_exit("signal",1);

	/* some more init */
	for(i=0;i<MAX_CPU;i++)
		shmem->val_cpu[i] = 0;

	/* main loop */
	while((lu = mygets(fd,buf,BUF_SIZE)) > 0)
	{
		if(dflag)
			fprintf(stderr,"buf[0] = %x\n<--\n%s\n-->\n",buf[0],buf);

		if(buf[1] == '.' || buf[lu-1] != '\n')
		{
			if(dflag)
				fprintf(stderr,"lu: %02d, ",lu);

			/* fix line with two 0x0d char */
			if((tmp = strchr(&buf[1],0x0d)) != NULL)
				lu = tmp-buf;

			/* skip line with proxy comm. */
			q = regexec(&preg_proxy,buf,1,pmatch,0);
			if(q != 0)
			{
				/* check if line match absolute crunch-o-meter */
				if(regexec(&preg_absolute,buf,1,pmatch,0) == 0)
				{
					/* set crunch-o-meter mode */
					shmem->cmode = CRUNCH_ABSOLUTE;
					/* read CPU num */
					i = strtol(&buf[pmatch[0].rm_so+1],(char **) NULL,10) - 1;
					/* avoid core dump */
					i %= MAX_CPU;
					/* read k(keys|nodes) */
					shmem->val_cpu[i] = extract_val(&buf[pmatch[0].rm_so]);
					
					if(dflag)
					{
						fprintf(stderr,"\ncpu = %d, %llu nodes|keys\n",i,shmem->val_cpu[i]);
						fprintf(stderr,"found: %s\n",&buf[pmatch[0].rm_so]);
					}
				}
				else
				{
					/* set crunch-o-meter mode */
					shmem->cmode = CRUNCH_RELATIVE;

					for(i=0;i<shmem->n_cpu;i++)
					{
						if(shmem->n_cpu != 1)
						{
							p[0] = 'a' + i;
							if((tmp = strstr(buf,p)) != NULL)
								pos_cpu[i] = tmp - buf;
						}
						else
							pos_cpu[i] = lu - 1;
						
						shmem->val_cpu[i] = (pos_cpu[i] - (pos_cpu[i]/8)*3) * 2;
						if(shmem->val_cpu[i] > 100)
							shmem->val_cpu[i] = 100;
						
						if(dflag)
							fprintf(stderr,"cpu%d: %llu,",i,shmem->val_cpu[i]);
					}
					if(dflag)
						fprintf(stderr,"\n");
				}
			}

			if(!qflag && (ttylog || q == 0))
			{
				printf("%s",buf); fflush(stdout);
			}
		}
		else
		{
			if(regexec(&preg_in,buf,1,pmatch,0) == 0)
				shmem->wu_in = strtol(&buf[pmatch[0].rm_so],NULL,10);
			if(regexec(&preg_out,buf,1,pmatch,0) == 0)
				shmem->wu_out = strtol(&buf[pmatch[0].rm_so],NULL,10);
			if(regexec(&preg_contest,buf,1,pmatch,0) == 0)
				strncpy(shmem->contest,&buf[pmatch[0].rm_so+contest_offset],3);
			if(regexec(&preg_cruncher,buf,1,pmatch,0) == 0)
			{
				shmem->n_cpu = strtol(&buf[pmatch[0].rm_so],NULL,10);
				/* too many crunchers */
				if(shmem->n_cpu > MAX_CPU)
				{
					fprintf(stderr,"dnetw: too many crunchers\n");
					clean_and_exit(NULL,1);
				}
			}

			if(!qflag)
			{
				printf("%s",buf); fflush(stdout);
			}
		}

	}

	clean_and_exit(NULL,0);

	return 0;
}
