/* $Id: gkrelldnet.c,v 1.29 2004-11-27 16:25:37 papier Exp $ */

/* GKrelldnet: a GKrellM plugin to monitor Distributed.net client
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "shmem.h"
#include "gkrelldnet.h"


/* Evil var! */
static GtkWidget *gkrellm_vbox;

/* gkrellm elements */
static GkrellmMonitor  *monitor;
static GkrellmPanel	*panel;
static GkrellmDecal    *decal_wu;
static GkrellmKrell    *krell_percent[MAX_CPU];
static gint		style_id;
static gboolean mouse_in;
static GtkTooltips *tooltip = NULL;

/* config widgets */
static GtkWidget *entry_format_str;
static GtkWidget *entry_stop_cmd, *entry_start_cmd;
static GtkWidget *entry_pck_done_cmd;
static GtkWidget *check_timeout_spin_button;

/* scroll vars */
static gint gk_width, separator_len;
/* buf. for CPU percentage */
static ulonglong buf_cpu_val[MAX_CPU];
/* buf nb of active CPU */
static int buf_n_cpu;

/* config. and monitored value */
struct dnetc_monitor
{
	gint check_timeout;                  /* sec. between update */
	gchar format_string[64];             /* output format string */
	gchar start_cmd[128];                /* start dnet client cmd */
	gchar stop_cmd[128];                 /* stop dnet client cmd */
	gchar pck_done_cmd[128];             /* packet done cmd */
	struct dnetc_values *shmem;          /* values from the wrapper */
};

/* default plugin config. */
static struct dnetc_monitor dnetmon = {
	2, "$c: $i/$o",
	"dnetw -q", "dnetc -quiet -shutdown",
	"",
	NULL
};

/* create a single krell */
static void create_krell(int num)
{
	GkrellmStyle      *style;
	GkrellmPiximage   *krell_image;

	style = gkrellm_meter_style(style_id);
	krell_image = gkrellm_krell_meter_piximage(style_id);

	krell_percent[num] = gkrellm_create_krell(panel, krell_image, style);
	gkrellm_monotonic_krell_values(krell_percent[num], FALSE);
	gkrellm_set_krell_full_scale(krell_percent[num], 100, 1);

	gkrellm_insert_krell(panel, krell_percent[num], FALSE);
}

/* create a krell for every active CPU */
static void create_percentage_krells()
{
	int i;

	for (i = 0; i < buf_n_cpu; i++)
		if (krell_percent[i] == NULL)
			create_krell(i);

	for (i = buf_n_cpu; i < MAX_CPU; i++)
		if (krell_percent[i] != NULL)
		{
			gkrellm_remove_krell(panel, krell_percent[i]);
			gkrellm_destroy_krell(krell_percent[i]);
			krell_percent[i] = NULL;
		}
}

/* update dnet values */
static void update_dnet2(void)
{
	int shmid,i;
	gchar tmp[128];	
	
	/* if shared memory not attached */
	if(dnetmon.shmem == NULL)
	{
		if((shmid = my_shmget(sizeof(struct dnetc_values),0444)) == -1)
			return;
		
		if((int) (dnetmon.shmem = shmat(shmid,0,0)) == -1)
		{
			dnetmon.shmem = NULL;
			return;
		}
	}
	else
	{
		/* detach shared memory if wrapper is not running */
		if(!dnetmon.shmem->running)
		{
			shmdt(dnetmon.shmem);
			dnetmon.shmem = NULL;
			for(i=0;i<MAX_CPU;i++)
		   		buf_cpu_val[i] = 0;
			buf_n_cpu = 1;
			create_percentage_krells();
		}
		else
		{
			/* check for packet completion */
			for(i=0;i<dnetmon.shmem->n_cpu;i++)
			{
				/* packet done */
				if(dnetmon.pck_done_cmd[0] != '\0'
				   && dnetmon.shmem->val_cpu[i] < buf_cpu_val[i])
				{
					strcpy(tmp,dnetmon.pck_done_cmd);
					g_spawn_command_line_async(tmp, NULL);
				}
				/* keep old value */
				buf_cpu_val[i] = dnetmon.shmem->val_cpu[i];
			}
			if (buf_n_cpu != dnetmon.shmem->n_cpu)
			{
				buf_n_cpu = dnetmon.shmem->n_cpu;
				create_percentage_krells();
			}
		}
	}
}

/* format cpu val with suffix depending on crunch-o-meter mode */
void sprint_cpu_val(char *buf,int max,guint64 val)
{
	gfloat tmp;

	/* add suffix depending on crunch-o-meter mode */
	switch(dnetmon.shmem->cmode)
	{
		case CRUNCH_RELATIVE:
			snprintf(buf,max,"%llu%%",val);
			break;
		case CRUNCH_ABSOLUTE:
			if(!strcmp(dnetmon.shmem->contest,"OGR"))
			{
				/* do auto-scale */
				tmp = (float) (val / 1000000ULL);
				snprintf(buf,max,"%.2f Gn",tmp/1000);
			}
			if(!strcmp(dnetmon.shmem->contest,"RC5"))
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

	if(dnetmon.shmem != NULL && dnetmon.shmem->contest[0] != '?')
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
						snprintf(buf,12,"%d",dnetmon.shmem->wu_in);
						s++;
						break;
					case 'o':
						snprintf(buf,12,"%d",dnetmon.shmem->wu_out);
						s++;
						break;
					case 'p':
						t = *(s+2) - '0';
						/* support up to 10 CPU */
						if(t >= 0 && t <= 9)
						{
							if(t < dnetmon.shmem->n_cpu)
								sprint_cpu_val(buf,24,dnetmon.shmem->val_cpu[t]);
							s++;
						}
						else
							sprint_cpu_val(buf,24,dnetmon.shmem->val_cpu[0]);
						s++;
						break;
					case 'c':
						snprintf(buf,12,"%s",dnetmon.shmem->contest);
						g_strdown(buf);
						s++;
						break;
					case 'C':
						snprintf(buf,12,"%s",dnetmon.shmem->contest);
						g_strup(buf);
						s++;
						break;
				}
			}
			strcat(text,buf);
		}
	}
	else
		snprintf(text,127,"dnet");
}

static void update_krells(void)
{
	int i;

	for (i = 0; i < buf_n_cpu; i++)
		if(dnetmon.shmem != NULL && krell_percent[i] != NULL
		   && dnetmon.shmem->cmode == CRUNCH_RELATIVE && !mouse_in)
			gkrellm_update_krell(panel, krell_percent[i], dnetmon.shmem->val_cpu[i]);
		else
			gkrellm_update_krell(panel, krell_percent[i], 0);
}		

static void update_plugin(void)
{
	static gint second_count = 0, x_scroll = 0, len = 0;
	static gchar text[128] = "dnet";
	static gchar full_text[256];

	if(GK.second_tick && (second_count++ % dnetmon.check_timeout) == 0)
	{
		update_dnet2();
		
		update_decals_text(text);
		sprintf(full_text,"%s   ***   %s",text,text);
		len = gkrellm_gdk_string_width(panel->textstyle->font,text);

		gtk_tooltips_set_tip(tooltip,panel->drawing_area,text,NULL);
		gtk_tooltips_set_delay(tooltip, 1000);

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
	gkrellm_draw_panel_layers(panel);
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

static gint cb_panel_enter(GtkWidget *w, GdkEventButton *ev)
{
	mouse_in = TRUE;
	return TRUE;
}

static gint cb_panel_leave(GtkWidget *w, GdkEventButton *ev)
{
	mouse_in = FALSE;
	return TRUE;
}

static gint cb_button_press(GtkWidget *widget, GdkEventButton *ev)
{
	gchar command[128];

	/* button 1 used (left button) */
	if(ev->button == 1)
		strcpy(command,dnetmon.start_cmd);
	
	/* button 2 used (middle button) */
	if(ev->button == 2)
		strcpy(command,dnetmon.stop_cmd);

	/* button 3 used (right button) */
	if(ev->button == 3)
		gkrellm_open_config_window(monitor);

	/* launch the command */
	if(ev->button == 1 || ev->button == 2)
		g_spawn_command_line_async(command, NULL);

	return FALSE;
}

static void create_plugin(GtkWidget *vbox, gint first_create)
{
	GkrellmStyle      *style;
	GkrellmMargin     *m;
	GkrellmTextstyle  *ts;
	GkrellmPiximage   *krell_image;
	gint y;
	gchar text[96];

	gkrellm_vbox = vbox;

	if (first_create)
		panel = gkrellm_panel_new0();

	style = gkrellm_meter_style(style_id);
	krell_image = gkrellm_krell_meter_piximage(style_id);
	ts = gkrellm_panel_textstyle(style_id);
	panel->textstyle = ts;

	create_krell(0);

	y = -1;
	decal_wu = gkrellm_create_decal_text(panel,"gd8", ts, style, -1, -1, -1);

    gkrellm_panel_configure(panel, NULL, style);
    gkrellm_panel_create(vbox, monitor, panel);

	/* Draw initial text in decals and krells */
	update_decals_text(text);
	gkrellm_draw_decal_text(panel,decal_wu,text,-1);
	update_krells();

	/* create Tooltip if necessary */
	if(tooltip == NULL) {
		tooltip = gtk_tooltips_new();
		gtk_tooltips_set_tip(tooltip,panel->drawing_area,text,NULL);
		gtk_tooltips_set_delay(tooltip, 1000);
	}

	if (first_create)
	{
	    gtk_signal_connect(GTK_OBJECT (panel->drawing_area), "expose_event",
    	        (GtkSignalFunc) panel_expose_event, NULL);
	    gtk_signal_connect(GTK_OBJECT (panel->drawing_area),
						   "button_press_event",
						   (GtkSignalFunc) cb_button_press, NULL);
		gtk_signal_connect(GTK_OBJECT(panel->drawing_area),
				"enter_notify_event", (GtkSignalFunc) cb_panel_enter, NULL);
		gtk_signal_connect(GTK_OBJECT(panel->drawing_area),
				"leave_notify_event", (GtkSignalFunc) cb_panel_leave, NULL);
	}

	gkrellm_draw_panel_layers(panel);

	/* some scroll init. */
	separator_len = gkrellm_gdk_string_width(panel->textstyle->font,"   ***   ");
	m = gkrellm_get_style_margins(style);
	gk_width = gkrellm_chart_width() - (m->left + m->right) - 2;
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
	"click start a new dnet client (see Start Command).\n",
	"<b>\tMiddle ",
	"click stop all dnet client (see Stop Command).\n",
	"<b>\tRight ",
	"open GKrellDnet plugin config window.\n\n",
	"<b>Configuration:\n\n",
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
	"\t\tdefault: dnetw -q\n\n",
	"<b>\tStop Command\n",
	"\t\tCommand line used to stop the dnet client on middle mouse button click\n",
	"\t\tdefault: dnetc -quiet -shutdown\n\n",
	"<b>\tPacket Completion Command\n",
	"\tCommand line executed each time a packet is done (default: none).\n"
	"\texample: 'esdplay /usr/share/sounds/a_nice_sound_that_I_love.wav'\n"
};

/* configuration tab */
static void create_dnet_tab(GtkWidget *tab)
{
	GtkWidget *tabs, *vbox, *hbox;
	GtkWidget *label, *frame;
	GtkWidget *text;
	GtkAdjustment *adj;
	gchar *about_text = NULL;
	int i;

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

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new("Format String");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
	entry_format_str = gtk_entry_new_with_max_length(127);
	gtk_entry_set_text(GTK_ENTRY(entry_format_str),dnetmon.format_string);
	gtk_box_pack_start(GTK_BOX(hbox), entry_format_str, FALSE, FALSE, 4);
	gtk_container_add(GTK_CONTAINER(vbox),hbox);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new("Start Command");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
	entry_start_cmd = gtk_entry_new_with_max_length(127);
	gtk_entry_set_text(GTK_ENTRY(entry_start_cmd),dnetmon.start_cmd);
	gtk_box_pack_start(GTK_BOX(hbox), entry_start_cmd, TRUE, TRUE, 4);
	gtk_container_add(GTK_CONTAINER(vbox),hbox);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new("Stop Command");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
	entry_stop_cmd = gtk_entry_new_with_max_length(127);
	gtk_entry_set_text(GTK_ENTRY(entry_stop_cmd),dnetmon.stop_cmd);
	gtk_box_pack_start(GTK_BOX(hbox), entry_stop_cmd, TRUE, TRUE, 4);
	gtk_container_add(GTK_CONTAINER(vbox),hbox);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new("Packet Completion Command");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
	entry_pck_done_cmd = gtk_entry_new_with_max_length(127);
	gtk_entry_set_text(GTK_ENTRY(entry_pck_done_cmd),dnetmon.pck_done_cmd);
	gtk_box_pack_start(GTK_BOX(hbox), entry_pck_done_cmd, TRUE, TRUE, 4);
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
	vbox = gkrellm_gtk_framed_notebook_page(tabs,"Info");
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	for (i = 0; i < sizeof(plugin_info_text)/sizeof(gchar *); i++)
		gkrellm_gtk_text_view_append(text, _(plugin_info_text[i]));
	
	/* about */
	about_text = g_strdup_printf(
		"GKrellDnet %s\n" \
		"GKrellM distributed.net Plugin\n\n" \
		"Copyright (C) 2000-2004 Laurent Papier\n" \
		"papier@tuxfan.net\n" \
		"http://gkrelldnet.sourceforge.net/\n\n" \
		"Released under the GNU Public Licence",
		GKRELLDNET_VERSION);
	
	text = gtk_label_new(about_text); 
	gtk_label_set_justify(GTK_LABEL(text), GTK_JUSTIFY_CENTER);
	label = gtk_label_new("About");
	gtk_notebook_append_page(GTK_NOTEBOOK(tabs),text,label);
	g_free(about_text);
}

static void save_config(FILE *f)
{
	fprintf(f,"%s check_timeout %d\n",CONFIG_KEYWORD,dnetmon.check_timeout);
	fprintf(f,"%s format_string %s\n",CONFIG_KEYWORD,dnetmon.format_string);
	fprintf(f,"%s start_command %s\n",CONFIG_KEYWORD,dnetmon.start_cmd);
	fprintf(f,"%s stop_command %s\n",CONFIG_KEYWORD,dnetmon.stop_cmd);
	fprintf(f,"%s packet_completion_cmd %s\n",CONFIG_KEYWORD,dnetmon.pck_done_cmd);
}

static void load_config(gchar *arg)
{
	gchar config[64],item[256];

	if(sscanf(arg,"%s %[^\n]",config,item) != 2 ) return ; 

	if(!strcmp("check_timeout",config))
		sscanf(item,"%d",&dnetmon.check_timeout);
	else if(!strcmp("format_string",config))
		strcpy(dnetmon.format_string,item);
	else if(!strcmp("start_command",config))
		strcpy(dnetmon.start_cmd,item);
	else if(!strcmp("stop_command",config))
		strcpy(dnetmon.stop_cmd,item);
	else if(!strcmp("packet_completion_cmd",config))
		strcpy(dnetmon.pck_done_cmd,item);
}

static void apply_config(void)
{
	int i;
	const gchar *s;

	/* update config vars */
	dnetmon.check_timeout = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(check_timeout_spin_button));
	s = gtk_entry_get_text(GTK_ENTRY(entry_format_str));
	strcpy(dnetmon.format_string,s);
	s = gtk_entry_get_text(GTK_ENTRY(entry_start_cmd));
	strcpy(dnetmon.start_cmd,s);
	s = gtk_entry_get_text(GTK_ENTRY(entry_stop_cmd));
	strcpy(dnetmon.stop_cmd,s);
	s = gtk_entry_get_text(GTK_ENTRY(entry_pck_done_cmd));
	strcpy(dnetmon.pck_done_cmd,s);
   
	/* delete old panel */
	if(panel != NULL)
	{
		gkrellm_panel_destroy(panel);
		panel = NULL;
		/* all krell are destroyed */
		for(i=0;i<MAX_CPU;i++)
			krell_percent[i] = NULL;
		buf_n_cpu = 1;
	}
	/* recreate panel */
	create_plugin(gkrellm_vbox,1);
}

/* The monitor structure tells GKrellM how to call the plugin routines.
*/
static GkrellmMonitor	plugin_mon	=
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


/* All GKrellM plugins must have one global routine named gkrellm_init_plugin()
|  which returns a pointer to a filled in monitor structure.
*/
GkrellmMonitor *gkrellm_init_plugin()
{
	int i;

	/* init cpu values buffer */
	for(i=0;i<MAX_CPU;i++)
	{
		buf_cpu_val[i] = 0;
		krell_percent[i] = NULL;
	}
	buf_n_cpu = 1;

	style_id = gkrellm_add_meter_style(&plugin_mon, STYLE_NAME);
	monitor = &plugin_mon;
	return &plugin_mon;
}
