/* dnetw: a distributed.net client wrapper
|  Copyright (C) 2000 Laurent Papier
|
|  Author:  Laurent Papier    papier@linuxfan.com
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#define MASTER_PTY "/dev/ptmx"

static int dflag = 0, qflag = 0;

/* pid of the dnet client */
static 	pid_t fils;
/* tty file descriptor */
static int fd = -1, tty_fd = -1;
/* cpu crunch-o-meter */
static int *percent_cpu = NULL, *pos_cpu = NULL;
/* monitor file fd */
static char monfile[128] = "";
static int mon_fd = -1;
/* regex */
static int regex_flag = 0;
static regex_t preg_in, preg_out, preg_contest;

/* clean exit function */
static void clean_and_exit(char *text,int r)
{
	if(text != NULL)
		perror(text);
	if(percent_cpu != NULL)
		free(percent_cpu);
	if(pos_cpu != NULL)
		free(pos_cpu);
	if(regex_flag)
	{
		regfree(&preg_in); regfree(&preg_out); regfree(&preg_contest);
	}
	if(mon_fd != -1)
	{
		close(mon_fd);
		if(unlink(monfile) == -1)
			perror("unlink");
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
	char buf[128];
	int lu;

	if(signum == SIGCHLD)
	{
		wait(NULL);
		clean_and_exit(NULL,0);
	}
	if(kill(fils,SIGQUIT) == -1)
		clean_and_exit("kill",1);

	if(!qflag)
	{
		while((lu = read(fd,buf,128)) > 0)
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
	
static void usage(char *pname)
{
	fprintf(stderr,"Distributed.net client wrapper 0.2\n");
	fprintf(stderr,"usage: %s [-q] [-l<log_file>] <monitor_file>\n",pname);
	fprintf(stderr," -q: disable terminal output\n");
	fprintf(stderr," -l<log_file>: redirect client output in <log_file>\n");
	clean_and_exit(NULL,0);
}

int main(int argc,char *argv[])
{
	extern char *optarg;
	extern int optind;
	int ch;

	char *ttydev,*tmp;
	char buf[128],buf1[64],buf2[16];
	int ttylog,lu,i;

	regmatch_t pmatch[2];

	char logfile[128] = "stdout";
	char contest[3] = "???";
	int log_fd,n_cpu = 1;
	int wu_in = 0,wu_out = 0;

	/* check arguments */
	while ((ch = getopt(argc, argv, "hdql:n:")) != -1)
	{
		switch(ch)
		{
			case 'd':
				dflag = 1;
				break;
			case 'q':
				qflag = 1;
				break;
			case 'n':
				n_cpu = atoi(optarg);
				break;
			case 'l':
				strcpy(logfile,optarg);
				break;
			default:
				usage(argv[0]);
		}
	}
	if(argc - optind != 1)
		usage(argv[0]);
	/* get monitor filename */
	strcpy(monfile,argv[argc-1]);

	/* change output to logfile */
	if(strcmp(logfile,"stdout") != 0)
	{
		if((log_fd = open(logfile, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1)
		clean_and_exit("open logfile",1);
		if(dup2(log_fd,1) == -1)
			clean_and_exit("dup2",1);
	}
	ttylog = isatty(1);
	if(dflag)
		fprintf(stderr,"ttylog = %d\n",ttylog);

	/* allocate some RAM ;-) */
	if((percent_cpu = (int *) calloc(n_cpu,sizeof(int))) == NULL)
		clean_and_exit("calloc",1);
	if((pos_cpu = (int *) calloc(n_cpu,sizeof(int))) == NULL)
		clean_and_exit("calloc",1);

	/* precompile regex */
	if(regcomp(&preg_in,"[0-9]+.work.unit.*buff-in",REG_EXTENDED) != 0)
		clean_and_exit(NULL,1);
	if(regcomp(&preg_out,"[0-9]+.work.unit.*buff-out",REG_EXTENDED) != 0)
		clean_and_exit(NULL,1);
	if(regcomp(&preg_contest,"Loaded.[A-Z0-9]{3}.",REG_EXTENDED) != 0)
		clean_and_exit(NULL,1);
	regex_flag = 1;

	/* first get a tty */
	if((fd = open(MASTER_PTY, O_RDWR)) == -1)
		clean_and_exit("open pty",1);

	grantpt(fd);
	unlockpt(fd);
	if((ttydev = (char *)ptsname(fd)) == NULL)
		clean_and_exit("ptsname",1);

	/* open the slave tty */
	if((tty_fd = open(ttydev, O_RDWR)) == -1)
		clean_and_exit("open tty",1);

	/* open monitor file */
	if(strcmp(monfile,"") != 0)
	{
		if((mon_fd = creat(monfile, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1)
		clean_and_exit("creat monitor file",1);
	}
	
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
		if(execlp("dnetc","dnetc",NULL) == -1)
			clean_and_exit("execlp",1);
	}

	/* set signal handler */
	if(signal(SIGHUP,got_signal) == SIG_ERR
	   || signal(SIGINT,got_signal) == SIG_ERR
	   || signal(SIGQUIT,got_signal) == SIG_ERR
	   || signal(SIGCHLD,got_signal) == SIG_ERR
	   || signal(SIGTERM,got_signal) == SIG_ERR)
		clean_and_exit("signal",1);
	   
	while((lu = read(fd,buf,128)) > 0)
	{
		buf[lu] = '\0';
		if(buf[1] == '.')
		{
			/*
			if((tmp = strstr(buf,"a")) != NULL)
				pos_a = tmp - buf - 1;
 			if((tmp = strstr(buf,"b")) != NULL)
				pos_b = tmp - buf - 1;
			*/

			if(dflag)
				fprintf(stderr,"lu: %02d, ",lu);
			for(i=0;i<n_cpu;i++)
			{
				pos_cpu[i] = lu - 1;
				percent_cpu[i] = (pos_cpu[i] - (pos_cpu[i]/8)*3) * 2;
				if(dflag)
					fprintf(stderr,"cpu%d: %d,",i,percent_cpu[i]);
			}
			if(dflag)
				fprintf(stderr,"\n");
			if(ttylog && !qflag)
			{
				printf("%s",buf); fflush(stdout);
			}
		}
		else
		{
			if(regexec(&preg_in,buf,1,pmatch,0) == 0)
				wu_in = strtol(&buf[pmatch[0].rm_so],NULL,10);
			else if(regexec(&preg_out,buf,1,pmatch,0) == 0)
				wu_out = strtol(&buf[pmatch[0].rm_so],NULL,10);
			else if(regexec(&preg_contest,buf,1,pmatch,0) == 0)
				strncpy(contest,&buf[pmatch[0].rm_so+7],3);

			if(!qflag)
				printf("%s",buf);
		}

		/* monitor output */
		sprintf(buf1,"%s %d %d %d",contest,wu_in,wu_out,n_cpu);
		for(i=0;i<n_cpu;i++)
		{
			sprintf(buf2," %d",percent_cpu[i]);
			strcat(buf1,buf2);
		}
		strcat(buf1,"\n");
		write(mon_fd,buf1,strlen(buf1));
	}

	clean_and_exit(NULL,0);
}

