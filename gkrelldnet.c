
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
static GtkWidget *entry_mon_file,*button_enable,*entry_format_str;
static gboolean dnet_enable = TRUE;

/* storage for dnet log values */
static gchar dnet_mon_file[256] = "/tmp/dnetw.mon";
static gchar dnet_format_string[128] = "$c: $i/$o";
static gint wu_in, wu_out, cpu_percent;
static gchar contest[3] = "???";


/*  update dnet values */
static void update_dnet(void)
{
	struct stat buf;
	FILE *fd;
	gchar tmp[128];
	gint pos;

	/* init. */
	wu_in = wu_out = 0;
	cpu_percent = 0;
	strcpy(contest,"???");

	/* get file size */
	if(stat(dnet_mon_file,&buf) == -1)
		return;

	/* open file for reading */
	if((fd = fopen(dnet_mon_file,"r")) == NULL)
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
			sscanf(tmp,"%s %d %d %d %d",contest,&wu_in,&wu_out,&pos,&cpu_percent);
	}

	fclose(fd);
}

/* update text in gkrellm decals */
static void update_decals_text(void)
{
	gchar *s,text[64],buf[12];

	if(contest[0] != '?')
	{
		text[0] = '\0';
		for(s=dnet_format_string; *s!='\0'; s++)
		{
			buf[0] = *s;
			buf[1] = '\0';
			if(*s == '$' && *(s+1) != '\0')
			{
				switch(*(s+1))
				{
					case 'i':
						snprintf(buf,12,"%d",wu_in);
						s++;
						break;
					case 'o':
						snprintf(buf,12,"%d",wu_out);
						s++;
						break;
					case 'p':
						snprintf(buf,12,"%d",cpu_percent);
						s++;
						break;
					case 'c':
						snprintf(buf,12,"%s",contest);
						g_strdown(buf);
						s++;
						break;
					case 'C':
						snprintf(buf,12,"%s",contest);
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

	gkrellm_draw_decal_text(panel,decal_wu,text,-1);
}

static void update_krells(void)
{
	krell_percent->previous = 0;
	gkrellm_update_krell(panel, krell_percent, cpu_percent);
}		

static void update_plugin(void)
{
	if(dnet_enable && GK.five_second_tick)
	{
		update_dnet();
		update_decals_text();
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
						panel->pixmap, ev->area.x, ev->area.y, ev->area.x, ev->area.y,
						ev->area.width, ev->area.height);
	}
	
	return FALSE;
}

static void
create_plugin(GtkWidget *vbox, gint first_create)
{
	Style			*style;
	TextStyle       *ts;
	GdkImlibImage   *krell_image;
	gint y;

	gkrellm_vbox = vbox;

	if(!dnet_enable)
		return;

	if (first_create)
		panel = gkrellm_panel_new0();
	else
	{
		gkrellm_destroy_decal_list(panel);
		gkrellm_destroy_krell_list(panel);
	}

	style = gkrellm_panel_style(style_id);
	krell_image = gkrellm_krell_panel_image(style_id);
	ts = gkrellm_panel_textstyle(style_id);
	panel->textstyle = ts;

    krell_percent = gkrellm_create_krell(panel, krell_image, style);
	krell_percent->full_scale = 100;

	y = -1;
	decal_wu = gkrellm_create_decal_text(panel,"gd8", ts, style, -1, -1, -1);

    gkrellm_configure_panel(panel, NULL, style);
    gkrellm_create_panel(vbox, panel, gkrellm_bg_panel_image(style_id));
    gkrellm_monitor_height_adjust(panel->h);

	/* Draw initial text in decals and krells */
	update_decals_text();
	update_krells();

	if (first_create)
	    gtk_signal_connect(GTK_OBJECT (panel->drawing_area), "expose_event",
    	        (GtkSignalFunc) panel_expose_event, NULL);

	gkrellm_draw_layers(panel);
}


/* configuration tab */
static void create_dnet_tab(GtkWidget *tab)
{
	GtkWidget *tabs, *vbox, *hbox;
	GtkWidget *label, *frame;
	GtkWidget *text;
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
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_enable),dnet_enable);
	gtk_container_add(GTK_CONTAINER(vbox),button_enable);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new("Format String");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
	entry_format_str = gtk_entry_new_with_max_length(127);
	gtk_entry_set_text(GTK_ENTRY(entry_format_str),dnet_format_string);
	gtk_box_pack_start(GTK_BOX(hbox), entry_format_str, FALSE, FALSE, 4);

	gtk_container_add(GTK_CONTAINER(vbox),hbox);
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new("Monitor File");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
	entry_mon_file = gtk_entry_new_with_max_length(255);
	gtk_entry_set_text(GTK_ENTRY(entry_mon_file),dnet_mon_file);
	gtk_box_pack_start(GTK_BOX(hbox), entry_mon_file, FALSE, FALSE, 4);

	gtk_container_add(GTK_CONTAINER(vbox),hbox);

	/* about */
	about_text = g_strdup_printf(
		"GKrellDnet %d.%d\n" \
		"GKrellM distributed.net Plugin\n\n" \
		"Copyright (C) 2000 Laurent Papier\n" \
		"papier@linuxfan.com\n" \
		"http://none.for.now ;-)\n\n" \
		"Released under the GNU Public Licence",
		DNET_MAJOR_VERSION,DNET_MINOR_VERSION);
	
	text = gtk_label_new(about_text); 
	label = gtk_label_new("About");
	gtk_notebook_append_page(GTK_NOTEBOOK(tabs),text,label);
	g_free(about_text);
}

static void save_config(FILE *f)
{
	fprintf(f,"%s enable %d\n",CONFIG_KEYWORD,dnet_enable);
	fprintf(f,"%s logfile %s\n",CONFIG_KEYWORD,dnet_mon_file);
	fprintf(f,"%s format_string %s\n",CONFIG_KEYWORD,dnet_format_string);
}

static void load_config(gchar *arg)
{
	gchar config[64],item[256];

	if(sscanf(arg,"%s %[^\n]",config,item) != 2 ) return ; 

	if(!strcmp("enable",config))
		sscanf(item,"%d",&dnet_enable);
	else if(!strcmp("logfile",config))
		strcpy(dnet_mon_file,item);
	else if(!strcmp("format_string",config))
		strcpy(dnet_format_string,item);
}

static void apply_config(void)
{
	gchar *s;

	/* update config vars */
	dnet_enable = GTK_TOGGLE_BUTTON(button_enable)->active;
	s = gtk_entry_get_text(GTK_ENTRY(entry_mon_file));
	strcpy(dnet_mon_file,s);
	s = gtk_entry_get_text(GTK_ENTRY(entry_format_str));
	strcpy(dnet_format_string,s);

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

	LOCATION,			/* Insert plugin before this monitor			*/

	NULL,				/* Handle if a plugin, filled in by GKrellM     */
	NULL				/* path if a plugin, filled in by GKrellM       */
};


  /* All GKrellM plugins must have one global routine named init_plugin()
  |  which returns a pointer to a filled in monitor structure.
  */
Monitor *
init_plugin()
{
	/* If this next call is made, the background and krell images for this
	|  plugin can be custom themed by putting bg_meter.png or krell.png in the
	|  subdirectory STYLE_NAME of the theme directory.  Text colors (and
	|  other things) can also be specified for the plugin with gkrellmrc
	|  lines like:  StyleMeter  STYLE_NAME.textcolor orange black shadow
	|  If no custom theming has been done, then all above calls using
	|  style_id will be equivalent to style_id = DEFAULT_STYLE_ID.
	*/
	style_id = gkrellm_add_meter_style(&plugin_mon, STYLE_NAME);
	return &plugin_mon;
}
