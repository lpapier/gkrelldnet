/* GKrelldnet
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex.h>
#include <errno.h>

#include "gkrelldnet.h"


#define LOOK_BACK 24*3


/* Evil var! */
static GtkWidget *gkrellm_vbox;

/* gkrellm elements */
static Panel	*panel;
static Decal    *decal_wu;
static Krell    *krell_percent;
static gint		style_id;

/* config widgets */
static GtkWidget *entry_mon_file,*button_enable;
static GtkWidget *entry_format_str, *entry_start_cmd;
static GtkWidget *entry_pck_done_cmd;
static GtkWidget *check_timeout_spin_button;

/* scroll vars */
static gint gk_width, separator_len;
/* buf. for CPU percentage */
static guint64 *buf_cpu_val;

/* config. and monitored value */
typedef struct
{
	gboolean enable;                     /* enable plugin */
	gint check_timeout;                  /* sec. between update */
	gint mode;                           /* crunch-o-meter mode */
	gchar file[256];                     /* monitoring file */
	gchar format_string[64];             /* output format string */
	gint n_cpu;                          /* num. of active crunchers */
	gint wu_in, wu_out;                  /* i/O work units */
	guint64 *cpu_val;                      /* cruncher value */
	gchar contest[4];                    /* current contest */
	gchar start_cmd[128];                /* start dnet client cmd */
	gchar stop_cmd[128];                 /* stop dnet client cmd */
	gchar pck_done_cmd[128];             /* packet done cmd */
} DnetMon;

/* default plugin config. */
static DnetMon dnetmon = {
	TRUE, 2, CRUNCH_RELATIVE,
	"/tmp/dnetw.mon", "$c: $i/$o",
	1,
	0, 0,
	NULL,
	"???",
	"dnetw -q", "dnetc -quiet -shutdown",
	""
};

/*  update dnet values */
static void update_dnet(void)
{
	struct stat buf;
	FILE *fd;
	guint64 *ti;
	gchar tmp[128], *t2;
	gint pos,ncpu,i;

	/* swap CPU value */
	ti = buf_cpu_val;
	buf_cpu_val = dnetmon.cpu_val;
	dnetmon.cpu_val = ti;
	
	/* init. */
	dnetmon.wu_in = dnetmon.wu_out = 0;
	for(i=0;i<dnetmon.n_cpu;i++)
		dnetmon.cpu_val[i] = 0;
	strcpy(dnetmon.contest,"???");

	/* get file size */
	if(stat(dnetmon.file,&buf) == -1)
		return;

	/* open file for reading */
	if((fd = fopen(dnetmon.file,"r")) == NULL)
		return;

	/* seek near the end of file */
	pos = buf.st_size - LOOK_BACK;
	if(pos < 0)
		pos = 0;

	if(fseek(fd,pos,SEEK_SET) == -1)
	{
		fclose(fd); return;
	}

	/* trash the first imcomplete line */
	if(fgets(tmp,128,fd) != NULL)
	{	
		/* read lines */
		while(fgets(tmp,128,fd) != NULL)
		{
			sscanf(tmp,"%s %d %d %d %d",dnetmon.contest,&dnetmon.mode,&dnetmon.wu_in,&dnetmon.wu_out,&ncpu);

			/* more CPU ! */
			if(ncpu > dnetmon.n_cpu)
			{
				/* free old RAM */
				free(dnetmon.cpu_val);
				/* allocate RAM for cpu monitoring */
				if((dnetmon.cpu_val = calloc(ncpu,sizeof(guint64))) == NULL
				   || (ti = calloc(ncpu,sizeof(guint64))) == NULL)
				{
					fclose(fd);
					return;
				}
				/* copy old CPU value */
				for(i=0;i<dnetmon.n_cpu;i++)
					ti[i] = buf_cpu_val[i];
				for(i=dnetmon.n_cpu;i<ncpu;i++)
					ti[i] = 0;
				/* free old mem */
				free(buf_cpu_val);
				buf_cpu_val = ti;
			}
			/* update number of crunchers */
			dnetmon.n_cpu = ncpu;
			/* read CPU val */
			for(i=dnetmon.n_cpu-1;i>=0;i--)
			{
				if((t2 = strrchr(tmp,' ')) != NULL)
				{
					sscanf(t2,"%llu",&dnetmon.cpu_val[i]);
					t2[0] = '\0';
				}
			}
		}
	}

	for(i=0;i<dnetmon.n_cpu;i++)
	{
		/* packet done */
		if(dnetmon.pck_done_cmd[0] != '\0'
		   && dnetmon.cpu_val[i] < buf_cpu_val[i])
		{
			strcpy(tmp,dnetmon.pck_done_cmd);
			strcat(tmp," &");
			system(tmp);
		}
	}

	fclose(fd);
}

/* format cpu val with suffix depending on crunch-o-meter mode */
void sprint_cpu_val(char *buf,int max,guint64 val)
{
	gfloat tmp;

	/* add suffix depending on crunch-o-meter mode */
	switch(dnetmon.mode)
	{
		case CRUNCH_RELATIVE:
			snprintf(buf,max,"%llu%",val);
			break;
		case CRUNCH_ABSOLUTE:
			if(!strcmp(dnetmon.contest,"OGR"))
			{
				/* do auto-scale */
				tmp = (float) (val / 1000000ULL);
				snprintf(buf,max,"%.2f Gn",tmp/1000);
			}
			if(!strcmp(dnetmon.contest,"RC5"))
			{
				/* do auto-scale */
				tmp = (float) (val / 1000ULL);
				snprintf(buf,max,"%.2f Mk",tmp/1000);
			}
			
			break;
	}
}


/* update text in gkrellm decals */
static void update_decals_text(gchar *text)
{
	gchar *s,buf[24];
	gint t;

	if(dnetmon.contest[0] != '?')
	{
		text[0] = '\0';
		for(s=dnetmon.format_string; *s!='\0'; s++)
		{
			buf[0] = *s;
			buf[1] = '\0';
			if(*s == '$' && *(s+1) != '\0')
			{
				switch(*(s+1))
				{
					case 'i':
						snprintf(buf,12,"%d",dnetmon.wu_in);
						s++;
						break;
					case 'o':
						snprintf(buf,12,"%d",dnetmon.wu_out);
						s++;
						break;
					case 'p':
						t = *(s+2) - '0';
						/* support up to 10 CPU */
						if(t >= 0 && t <= 9)
						{
							if(t < dnetmon.n_cpu)
								sprint_cpu_val(buf,24,dnetmon.cpu_val[t]);
							s++;
						}
						else
							sprint_cpu_val(buf,24,dnetmon.cpu_val[0]);
						s++;
						break;
					case 'c':
						snprintf(buf,12,"%s",dnetmon.contest);
						g_strdown(buf);
						s++;
						break;
					case 'C':
						snprintf(buf,12,"%s",dnetmon.contest);
						g_strup(buf);
						s++;
						break;
				}
			}
			strcat(text,buf);
		}
	}
	else
		strcpy(text,"dnet");
}

static void update_krells(void)
{
	krell_percent->previous = 0;
	if(dnetmon.mode == CRUNCH_RELATIVE)
		gkrellm_update_krell(panel, krell_percent, dnetmon.cpu_val[0]);
	else
		gkrellm_update_krell(panel, krell_percent, 0);
}		

static void update_plugin(void)
{
	static gint second_count = 0, x_scroll = 0, len = 0;
	static gchar text[128] = "dnet";
	static gchar full_text[256];

	if(dnetmon.enable)
	{
		if(GK.second_tick && (second_count++ % dnetmon.check_timeout) == 0)
		{
			update_dnet();

			update_decals_text(text);
			sprintf(full_text,"%s   ***   %s",text,text);
			len = gdk_string_width(panel->textstyle->font,text);

			update_krells();
		}

		if(len > gk_width)
		{
			x_scroll = (x_scroll + 1) % (len + separator_len);
			decal_wu->x_off = gk_width - x_scroll - len;
			gkrellm_draw_decal_text(panel,decal_wu,full_text,-1);
		}
		else
		{
			x_scroll = decal_wu->x_off = 0;
			gkrellm_draw_decal_text(panel,decal_wu,text,-1);
		}

		update_krells();
		gkrellm_draw_layers(panel);
	}
}

static gint panel_expose_event(GtkWidget *widget, GdkEventExpose *ev)
{
	if (widget == panel->drawing_area)
	{
		gdk_draw_pixmap(widget->window,
						widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
						panel->pixmap,
						ev->area.x, ev->area.y,
						ev->area.x, ev->area.y,
						ev->area.width, ev->area.height);
	}
	
	return FALSE;
}

static gint cb_button_press(GtkWidget *widget, GdkEventButton *ev)
{
	gchar command[128];

	/* button 1 used (left button) */
	if(ev->button == 1)
	{
		strcpy(command,dnetmon.start_cmd);
		strcat(command," ");
		strcat(command,dnetmon.file);
	}
		
	/* button 3 used (right button) */
	if(ev->button == 3)
		strcpy(command,dnetmon.stop_cmd);

	/* launch the command */
	if(ev->button == 1 || ev->button == 3)
	{
		strcat(command," &");
		system(command);
	}
}

static void
create_plugin(GtkWidget *vbox, gint first_create)
{
	Style			*style;
	TextStyle       *ts;
	GdkImlibImage   *krell_image;
	gint y;
	gchar text[96] = "dnet";

	gkrellm_vbox = vbox;

	if(!dnetmon.enable)
		return;

	if (first_create)
		panel = gkrellm_panel_new0();
	else
	{
		gkrellm_destroy_decal_list(panel);
		gkrellm_destroy_krell_list(panel);
	}

	style = gkrellm_meter_style(style_id);
	krell_image = gkrellm_krell_meter_image(style_id);
	ts = gkrellm_panel_textstyle(style_id);
	panel->textstyle = ts;

    krell_percent = gkrellm_create_krell(panel, krell_image, style);
	krell_percent->full_scale = 100;

	y = -1;
	decal_wu = gkrellm_create_decal_text(panel,"gd8", ts, style, -1, -1, -1);

    gkrellm_configure_panel(panel, NULL, style);
    gkrellm_create_panel(vbox, panel, gkrellm_bg_meter_image(style_id));
    gkrellm_monitor_height_adjust(panel->h);

	/* Draw initial text in decals and krells */
	update_decals_text(text);
	gkrellm_draw_decal_text(panel,decal_wu,text,-1);
	update_krells();

	if (first_create)
	{
	    gtk_signal_connect(GTK_OBJECT (panel->drawing_area), "expose_event",
    	        (GtkSignalFunc) panel_expose_event, NULL);
	    gtk_signal_connect(GTK_OBJECT (panel->drawing_area),
						   "button_press_event",
						   (GtkSignalFunc) cb_button_press, NULL);
	}

	gkrellm_draw_layers(panel);

	/* some scroll init. */
	separator_len = gdk_string_width(panel->textstyle->font,"   ***   ");
	gk_width = gkrellm_chart_width() - (2 * style->margin) - 2;
}

static gchar *plugin_info_text[] = {
	"<b>GKrellDnet ",
	"is a GKrellM plugin which allow you to monitor your\n",
	"distributed.net client. It features:\n\n",
	"\t- monitoring of work units present in both input and output buffers.\n",
	"\t- monitoring of percentage done in current block/stub.\n",
	"\t- monitoring of current contest.\n",
	"\t- configurable output format.\n",
	"\t- execute a program after each packet is done.\n",
	"\t- start/stop dnet client on mouse button click.\n\n",
	"<b>Mouse Button Actions:\n\n",
	"<b>\tLeft ",
	"click start a new dnet client (default: dnetw -q <monitor file>).\n",
	"<b>\tRight ",
	"click stop all dnet client (default: dnetc -quiet -shutdown).\n\n",
	"<b>Configuration:\n\n",
	"<b>\tEnable Distributed.net monitor\n",
	"\tIf you want to disable this plugin (default: enable).\n\n",
	"<b>\tFormat String\n",
	"\tThe text output format is controlled by this string (default: $c: $i/$o).\n",
	"<b>\t\t$i ", "is the input work units\n",
	"<b>\t\t$o ", "is the output work units\n",
	"<b>\t\t$c/$C ", "is the current contest in lowercase/uppercase\n",
	"<b>\t\t$p", "<i>n ",
	"is the progress indicator value in current block/stub on CPU #",
	"<i>n", " ($p <=> $p0)\n\n",
	"\tThe plugin now automaticaly add a suffix depending on the crunch-o-meter mode:\n",
	"<b>\t\tMk", " (Mkeys) is added for RC5 contest in absolute mode.\n",
	"<b>\t\tGn", " (Gnodes) is added for OGR contest in absolute mode.\n",
	"<b>\t\t%", " is added in relative mode.\n\n",
	"<b>\tStart Command\n",
	"\t\tCommand line used to start the dnet client on left mouse button click\n",
	"<b>\t\tDo not add the monitor file in the command. ",
	"The plugin will do it for you.\n\n",
	"<b>\tMonitor File\n",
	"\tSet the file used by the Distributed.net client wrapper 'dnetw' to communicate\n",
	"\twith monitoring applications (default: /tmp/dnetw.log).\n\n",
	"<b>\tPacket Completion Command\n",
	"\tCommand line executed each time a packet is done (default: none).\n"
	"\texample: 'esdplay /usr/share/sounds/a_nice_sound_that_I_love.wav'\n"
};

/* configuration tab */
static void create_dnet_tab(GtkWidget *tab)
{
	GtkWidget *tabs, *vbox, *hbox;
	GtkWidget *label, *frame;
	GtkWidget *scrolled, *text;
	GtkAdjustment *adj;
	gchar *about_text = NULL;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs),GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab),tabs,TRUE,TRUE,0);  

	/* options */
	frame = gtk_frame_new(NULL);
	gtk_container_border_width(GTK_CONTAINER(frame),3);
	label = gtk_label_new("Options");
	gtk_notebook_append_page(GTK_NOTEBOOK(tabs),frame,label);  
 
	vbox = gtk_vbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(frame),vbox);

	button_enable = gtk_check_button_new_with_label("Enable Distributed.net monitor");  
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_enable),dnetmon.enable);
	gtk_container_add(GTK_CONTAINER(vbox),button_enable);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new("Format String");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
	entry_format_str = gtk_entry_new_with_max_length(127);
	gtk_entry_set_text(GTK_ENTRY(entry_format_str),dnetmon.format_string);
	gtk_box_pack_start(GTK_BOX(hbox), entry_format_str, FALSE, FALSE, 4);
	gtk_container_add(GTK_CONTAINER(vbox),hbox);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new("Packet Completion Command");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
	entry_pck_done_cmd = gtk_entry_new_with_max_length(127);
	gtk_entry_set_text(GTK_ENTRY(entry_pck_done_cmd),dnetmon.pck_done_cmd);
	gtk_box_pack_start(GTK_BOX(hbox), entry_pck_done_cmd, TRUE, TRUE, 4);
	gtk_container_add(GTK_CONTAINER(vbox),hbox);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new("Start Command");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
	entry_start_cmd = gtk_entry_new_with_max_length(127);
	gtk_entry_set_text(GTK_ENTRY(entry_start_cmd),dnetmon.start_cmd);
	gtk_box_pack_start(GTK_BOX(hbox), entry_start_cmd, TRUE, TRUE, 4);
	gtk_container_add(GTK_CONTAINER(vbox),hbox);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new("Monitor File");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
	entry_mon_file = gtk_entry_new_with_max_length(255);
	gtk_entry_set_text(GTK_ENTRY(entry_mon_file),dnetmon.file);
	gtk_box_pack_start(GTK_BOX(hbox), entry_mon_file, FALSE, FALSE, 4);
	gtk_container_add(GTK_CONTAINER(vbox),hbox);

	hbox = gtk_hbox_new(FALSE, 0);
	adj = (GtkAdjustment *) gtk_adjustment_new(dnetmon.check_timeout,
												1.0, 30.0, 1.0, 5.0, 0.0);
	check_timeout_spin_button = gtk_spin_button_new(adj, 0.5, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(check_timeout_spin_button), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), check_timeout_spin_button, FALSE, FALSE, 4);
	label = gtk_label_new("Seconds between data updates");
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
	gtk_container_add(GTK_CONTAINER(vbox),hbox);

	/* info */
	vbox = create_tab(tabs,"Info");
	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
	text = gtk_text_new(NULL, NULL);
	gkrellm_add_info_text(text, plugin_info_text,
			sizeof(plugin_info_text) / sizeof(gchar *));
	gtk_text_set_editable(GTK_TEXT(text), FALSE);
	gtk_container_add(GTK_CONTAINER(scrolled), text);
	
	/* about */
	about_text = g_strdup_printf(
		"GKrellDnet %s\n" \
		"GKrellM distributed.net Plugin\n\n" \
		"Copyright (C) 2001 Laurent Papier\n" \
		"papier@linuxfan.com\n" \
		"http://gkrelldnet.sourceforge.net/\n\n" \
		"Released under the GNU Public Licence",
		GKRELLDNET_VERSION);
	
	text = gtk_label_new(about_text); 
	label = gtk_label_new("About");
	gtk_notebook_append_page(GTK_NOTEBOOK(tabs),text,label);
	g_free(about_text);
}

static void save_config(FILE *f)
{
	fprintf(f,"%s enable %d\n",CONFIG_KEYWORD,dnetmon.enable);
	fprintf(f,"%s check_timeout %d\n",CONFIG_KEYWORD,dnetmon.check_timeout);
	fprintf(f,"%s logfile %s\n",CONFIG_KEYWORD,dnetmon.file);
	fprintf(f,"%s format_string %s\n",CONFIG_KEYWORD,dnetmon.format_string);
	fprintf(f,"%s start_command %s\n",CONFIG_KEYWORD,dnetmon.start_cmd);
	fprintf(f,"%s packet_completion_cmd %s\n",CONFIG_KEYWORD,dnetmon.pck_done_cmd);
}

static void load_config(gchar *arg)
{
	gchar config[64],item[256];

	if(sscanf(arg,"%s %[^\n]",config,item) != 2 ) return ; 

	if(!strcmp("enable",config))
		sscanf(item,"%d",&dnetmon.enable);
	else if(!strcmp("check_timeout",config))
		sscanf(item,"%d",&dnetmon.check_timeout);
	else if(!strcmp("logfile",config))
		strcpy(dnetmon.file,item);
	else if(!strcmp("format_string",config))
		strcpy(dnetmon.format_string,item);
	else if(!strcmp("start_command",config))
		strcpy(dnetmon.start_cmd,item);
	else if(!strcmp("packet_completion_cmd",config))
		strcpy(dnetmon.pck_done_cmd,item);
}

static void apply_config(void)
{
	gchar *s;

	/* update config vars */
	dnetmon.enable = GTK_TOGGLE_BUTTON(button_enable)->active;
	dnetmon.check_timeout = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(check_timeout_spin_button));
	s = gtk_entry_get_text(GTK_ENTRY(entry_mon_file));
	strcpy(dnetmon.file,s);
	s = gtk_entry_get_text(GTK_ENTRY(entry_format_str));
	strcpy(dnetmon.format_string,s);
	s = gtk_entry_get_text(GTK_ENTRY(entry_start_cmd));
	strcpy(dnetmon.start_cmd,s);
	s = gtk_entry_get_text(GTK_ENTRY(entry_pck_done_cmd));
	strcpy(dnetmon.pck_done_cmd,s);
   
	/* delete old panel */
	if(panel != NULL)
	{
		gkrellm_monitor_height_adjust(- panel->h);
		gkrellm_destroy_decal_list(panel);
		gkrellm_destroy_krell_list(panel);
		gkrellm_destroy_panel(panel);
		g_free(panel);
		panel = NULL;
	}
	/* create new panel */
	create_plugin(gkrellm_vbox,1);
}

/* The monitor structure tells GKrellM how to call the plugin routines.
*/
static Monitor	plugin_mon	=
{
	CONFIG_NAME,        /* Title for config clist.   */
	0,					/* Id,  0 if a plugin       */
	create_plugin,		/* The create function      */
	update_plugin,		/* The update function      */
	create_dnet_tab,	/* The config tab create function   */
	apply_config,				/* Apply the config function        */

	save_config,				/* Save user config			*/
	load_config,				/* Load user config			*/
	CONFIG_KEYWORD,		/* config keyword			*/

	NULL,				/* Undefined 2	*/
	NULL,				/* Undefined 1	*/
	NULL,				/* private	*/

	MY_PLACEMENT,			/* Insert plugin before this monitor			*/

	NULL,				/* Handle if a plugin, filled in by GKrellM     */
	NULL				/* path if a plugin, filled in by GKrellM       */
};


  /* All GKrellM plugins must have one global routine named init_plugin()
  |  which returns a pointer to a filled in monitor structure.
  */
Monitor *
init_plugin()
{
	int i;

	/* allocate RAM for cpu monitoring */
	if((dnetmon.cpu_val = calloc(dnetmon.n_cpu,sizeof(guint64))) == NULL
	   || (buf_cpu_val = calloc(dnetmon.n_cpu,sizeof(guint64))) == NULL)
		return NULL;
	for(i=0;i<dnetmon.n_cpu;i++)
	{
		dnetmon.cpu_val[i] = 0;
		buf_cpu_val[i] = 0;
	}

	style_id = gkrellm_add_meter_style(&plugin_mon, STYLE_NAME);
	return &plugin_mon;
}
