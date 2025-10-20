#include <gtk/gtk.h>
#include <jansson.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>
#include <stdbool.h>
#include "api.h"
#include "deadbeef_util.h"

DB_functions_t *deadbeef;
ddb_gtkui_t *gtkui_plugin;

//#define MAX(a,b) ((a)>=(b)? (a) : (b))

#define CONFIG_DIALOG_COUNT "widgetdialog.count"
#define CONFIGPREFIX_LAYOUT "widgetdialog.layout"
#define CONFIGPREFIX_WIDTH  "widgetdialog.width"
#define CONFIGPREFIX_HEIGHT "widgetdialog.height"
#define CONFIGPREFIX_DIALOG_TITLE "widgetdialog.windowtitle"
#define CONFIGPREFIX_ACTION_TITLE "widgetdialog.actiontitle"

#define CONFIGPREFIX_MAX_LEN \
	MAX(sizeof(CONFIGPREFIX_LAYOUT),\
	MAX(sizeof(CONFIGPREFIX_WIDTH),\
	MAX(sizeof(CONFIGPREFIX_HEIGHT),\
	MAX(sizeof(CONFIGPREFIX_DIALOG_TITLE),\
	    sizeof(CONFIGPREFIX_ACTION_TITLE)\
	))))

//An instance is additional data to an action.
//All actions in this plugin should always be attached to a struct instance_s.
struct instance_s{
	GtkDialog *dialog;
	ddb_gtkui_widget_t *root_container;
	char config_id[50];
	char action_id[50];
	char action_title[100];
	char dialog_title[100];
	int width;
	int height;
	DB_plugin_action_t action;
};

struct widgetdialog_s{
	struct instance_s *insts;
} widgetdialog;

//Only for the actions in this plugin.
//Okay because they should all be part of the larger struct instance_s.
static inline struct instance_s *action_get_instance(DB_plugin_action_t *ptr){return (struct instance_s*)(((unsigned char*)ptr)-offsetof(struct instance_s,action));}

//Just uses the linked list of the action.
static inline struct instance_s *instance_next(struct instance_s *inst){return inst->action.next? action_get_instance(inst->action.next) : NULL;}

static bool widgetdialog_on_resize(__attribute__((unused)) GtkWidget* self,__attribute__((unused)) GdkEventConfigure *event,gpointer user_data){
	struct instance_s *inst = (struct instance_s*)user_data;
	//TODO: also possible to use width/height of event
	gtk_window_get_size(GTK_WINDOW(inst->dialog),&inst->width,&inst->height);
	return false;
}

static void instance_save(struct instance_s *inst){
	char buffer[CONFIGPREFIX_MAX_LEN + 1 + sizeof(inst->config_id) + 1];

	snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_WIDTH,inst->config_id);
	deadbeef->conf_set_int(buffer ,inst->width);

	snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_HEIGHT,inst->config_id);
	deadbeef->conf_set_int(buffer,inst->height);

	snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_LAYOUT,inst->config_id);
	rootwidget_save(inst->root_container,buffer);

	snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_DIALOG_TITLE,inst->config_id);
	deadbeef->conf_set_str(buffer,inst->dialog_title);

	snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_ACTION_TITLE,inst->config_id);
	deadbeef->conf_set_str(buffer,inst->action_title);
}

static void widgetdialog_on_dialog_destroy(__attribute__((unused)) GtkDialog* self,gpointer user_data){
	struct instance_s *inst = (struct instance_s*)user_data;

	instance_save(inst);

	for(ddb_gtkui_widget_t *c = inst->root_container->children; c; c = c->next){
		gtkui_plugin->w_remove(inst->root_container,c);
		gtkui_plugin->w_destroy(c);
	}

	gtkui_plugin->w_destroy(inst->root_container);
	inst->root_container = NULL;
	inst->dialog = NULL;
}

static int widgetdialog_dialog_create(void *user_data){
	struct instance_s *inst = (struct instance_s*)user_data;

	GtkWidget *main_window = gtkui_plugin->get_mainwin();
	if(!main_window) return G_SOURCE_REMOVE;

	inst->dialog = GTK_DIALOG(gtk_dialog_new());
		gtk_window_set_transient_for(GTK_WINDOW(inst->dialog),GTK_WINDOW(main_window));
		gtk_window_set_default_size(GTK_WINDOW(inst->dialog),inst->width,inst->height);
		gtk_window_set_title(GTK_WINDOW(inst->dialog),inst->dialog_title);
		gtk_window_set_modal(GTK_WINDOW(inst->dialog),false);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(inst->dialog),true);
		g_signal_connect(G_OBJECT(inst->dialog),"destroy",G_CALLBACK(widgetdialog_on_dialog_destroy),inst);
		g_signal_connect(G_OBJECT(inst->dialog),"configure-event",G_CALLBACK(widgetdialog_on_resize),inst);

		//Root widget.
		char buffer[sizeof(CONFIGPREFIX_LAYOUT) + 1 + sizeof(inst->config_id) + 1];
		snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_LAYOUT,inst->config_id);
		rootwidget_init(&inst->root_container,buffer);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(inst->dialog)),inst->root_container->widget,true,true,0);

		gtk_widget_show(GTK_WIDGET(inst->dialog));
	return G_SOURCE_REMOVE;
}

static int widgetdialog_dialog_destroy(void *user_data){
	struct instance_s *inst = (struct instance_s*)user_data;
	gtk_widget_destroy(GTK_WIDGET(inst->dialog));
	return G_SOURCE_REMOVE;
}

static int action_toggle_dialog_callback(DB_plugin_action_t *action,__attribute__ ((unused)) ddb_action_context_t ctx){
	struct instance_s *inst = action_get_instance(action);
	g_idle_add(inst->dialog? widgetdialog_dialog_destroy : widgetdialog_dialog_create,inst);
	return 0;
}

static struct instance_s *instance_add(const char *id){
	struct instance_s *inst = malloc(sizeof(struct instance_s));
	if(!inst) return NULL;

	//Append to the linked list.
	if(!widgetdialog.insts){
		widgetdialog.insts = inst;
	}else{
		DB_plugin_action_t **p = &widgetdialog.insts->action.next;
		while(*p){p = &(*p)->next;};
		*p = &inst->action;
	}

	inst->dialog = NULL;
	inst->root_container = NULL;

	snprintf(inst->config_id,sizeof(inst->config_id),"%s",id);
	snprintf(inst->action_id,sizeof(inst->action_id),"widgetdialog_toggle_%s",id);

	{
		char buffer[CONFIGPREFIX_MAX_LEN + 1 + sizeof(inst->config_id) + 1];

		snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_WIDTH,inst->config_id);
		inst->width = deadbeef->conf_get_int(buffer,256);
		if(inst->width < 16) inst->width = 16;

		snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_HEIGHT,inst->config_id);
		inst->height = deadbeef->conf_get_int(buffer,256);
		if(inst->height < 16) inst->height = 16;

		deadbeef->conf_lock();
		{
			snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_ACTION_TITLE,inst->config_id);
			const char *s = deadbeef->conf_get_str_fast(buffer,NULL);
			if(s){snprintf(inst->action_title,sizeof(inst->action_title),"%s",s);}
			else {snprintf(inst->action_title,sizeof(inst->action_title),"View/Dialog %s",id);}
		}{
			snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_DIALOG_TITLE,inst->config_id);
			const char *s = deadbeef->conf_get_str_fast(buffer,NULL);
			if(s){snprintf(inst->dialog_title,sizeof(inst->dialog_title),"%s",s);}
			else {snprintf(inst->dialog_title,sizeof(inst->dialog_title),"Dialog %s",id);}
		}
		deadbeef->conf_unlock();
	}

	inst->action = (DB_plugin_action_t){
		.title = inst->action_title,
		.name = inst->action_id,
		.flags = DB_ACTION_COMMON | DB_ACTION_ADD_MENU,
		.callback2 = action_toggle_dialog_callback,
		.next = NULL
	};

	return inst;
}

static int widgetdialog_start(){
	widgetdialog.insts = NULL;

	for(int i=0; i<deadbeef->conf_get_int(CONFIG_DIALOG_COUNT,0); i+=1){
		//TODO: just a temporary solution
		char buffer[1+20 + 1]; snprintf(buffer,sizeof(buffer),"d%d",i);
		instance_add(buffer);
	}

	return 0;
}

static int widgetdialog_connect(void){
	if(!(gtkui_plugin = (ddb_gtkui_t*) deadbeef->plug_get_for_id(DDB_GTKUI_PLUGIN_ID))){
		return -1;
	}
	//gtkui_plugin->add_window_init_hook(widgetdialog_window_init_hook,NULL);

	return 0;
}

static int widgetdialog_message(uint32_t id,__attribute__((unused)) uintptr_t ctx,__attribute__((unused)) uint32_t p1,__attribute__((unused)) uint32_t p2){
	switch(id){
		case DB_EV_TERMINATE:
			//TODO: Only saves when it is open and application terminates
			for(struct instance_s *inst = widgetdialog.insts; inst; inst = instance_next(inst)){
				instance_save(inst);
			}
			break;
	}

	for(struct instance_s *inst = widgetdialog.insts; inst; inst = instance_next(inst)){
		if(inst->root_container) send_messages_to_widgets(inst->root_container,id,ctx,p1,p2);
	}
	return 0;
}

static DB_plugin_action_t *widgettoggle_get_actions(__attribute__((unused)) DB_playItem_t *it){
	return widgetdialog.insts? &widgetdialog.insts->action : NULL;
}

static const char settings_dlg[] =
	"property \"Dialogs (requires restart)\" spinbtn[0,40,1] " CONFIG_DIALOG_COUNT " 0;"
;

static struct instance_s *api_get_instance(const char *config_id){
	for(struct instance_s *inst = widgetdialog.insts; inst; inst = instance_next(inst)){
		if(strcmp(config_id,inst->config_id) == 0) return inst;
	}
	return NULL;
}
static GtkDialog          *api_instance_get_dialog      (struct instance_s *inst){return inst->dialog;}
static ddb_gtkui_widget_t *api_instance_get_rootwidget  (struct instance_s *inst){return inst->root_container;}
static const char         *api_instance_get_config_id   (struct instance_s *inst){return inst->config_id;}
static const char         *api_instance_get_action_id   (struct instance_s *inst){return inst->action_id;}
static const char         *api_instance_get_action_title(struct instance_s *inst){return inst->action_title;}
static const char         *api_instance_get_dialog_title(struct instance_s *inst){return inst->dialog_title;}
static ddb_widgetdialog_t plugin ={
	.misc.plugin.api_vmajor = DB_API_VERSION_MAJOR,
	.misc.plugin.api_vminor = DB_API_VERSION_MINOR,
	.misc.plugin.version_major = 1,
	.misc.plugin.version_minor = 0,
	.misc.plugin.type = DB_PLUGIN_MISC,
	#if GTK_CHECK_VERSION(3,0,0)
	.misc.plugin.id = "widgetdialog_gtk3",
	#else
	.misc.plugin.id = "widgetdialog_gtk2",
	#endif
	.misc.plugin.name = "Widget Dialog",
	.misc.plugin.descr =
		"Customisable widget dialogs.\n"
		"Set the number of dialogs in the plugin configuration. Restart for changes.\n"
		"Open a dialog by using the actions found in \"View/Dialog *\" (widgetdialog_toggle_*).\n"
		"In the new dialog, widgets can be added and modified in Design Mode.\n"
		"To save the layout changes made in a dialog, the window must be closed while in Design Mode.\n"
		"\n"
		"This plugin is not finished as of writing, but should be functional.\n"
	,
	.misc.plugin.copyright =
		"MIT License\n"
		"\n"
		"Copyright 2025 EDT4\n"
		"\n"
		"Permission is hereby granted,free of charge,to any person obtaining a copy\n"
		"of this software and associated documentation files(the \"Software\"),to deal\n"
		"in the Software without restriction,including without limitation the rights\n"
		"to use,copy,modify,merge,publish,distribute,sublicense,and/or sell\n"
		"copies of the Software,and to permit persons to whom the Software is\n"
		"furnished to do so,subject to the following conditions:\n"
		"\n"
		"The above copyright notice and this permission notice shall be included in all\n"
		"copies or substantial portions of the Software.\n"
		"\n"
		"THE SOFTWARE IS PROVIDED \"AS IS\",WITHOUT WARRANTY OF ANY KIND,EXPRESS OR\n"
		"IMPLIED,INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
		"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
		"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,DAMAGES OR OTHER\n"
		"LIABILITY,WHETHER IN AN ACTION OF CONTRACT,TORT OR OTHERWISE,ARISING FROM,\n"
		"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
		"SOFTWARE.\n"
	,
	.misc.plugin.website = "https://github.org/EDT4/ddb_widgetdialog",
	.misc.plugin.connect = widgetdialog_connect,
	.misc.plugin.start   = widgetdialog_start,
	.misc.plugin.message = widgetdialog_message,
	.misc.plugin.get_actions = &widgettoggle_get_actions,
	.misc.plugin.configdialog = settings_dlg,
	.get_instance              = api_get_instance,
	.instance_get_dialog       = api_instance_get_dialog,
	.instance_get_rootwidget   = api_instance_get_rootwidget,
	.instance_get_config_id    = api_instance_get_config_id,
	.instance_get_action_id    = api_instance_get_action_id,
	.instance_get_action_title = api_instance_get_action_title,
	.instance_get_dialog_title = api_instance_get_dialog_title,
};

__attribute__((visibility("default")))
DB_plugin_t *
#if GTK_CHECK_VERSION(3,0,0)
widgetdialog_gtk3_load
#else
widgetdialog_gtk2_load
#endif
(DB_functions_t *api){
	deadbeef = api;
	return DB_PLUGIN(&plugin);
}
