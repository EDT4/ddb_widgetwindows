#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>
#include <stdbool.h>
#include "api.h"
#include "deadbeef_util.h"

DB_functions_t *deadbeef;
ddb_gtkui_t *gtkui_plugin;

//#define MAX(a,b) ((a)>=(b)? (a) : (b))

#define CONFIG_WINDOW_COUNT "widgetwindows.count"
#define CONFIGPREFIX_LAYOUT "widgetwindows.layout"
#define CONFIGPREFIX_WIDTH  "widgetwindows.width"
#define CONFIGPREFIX_HEIGHT "widgetwindows.height"
#define CONFIGPREFIX_FLAGS  "widgetwindows.flags"
#define CONFIGPREFIX_WINDOW_TITLE "widgetwindows.windowtitle"
#define CONFIGPREFIX_ACTION_TITLE "widgetwindows.actiontitle"

//Used for buffers containing a config key.
#define CONFIGPREFIX_MAX_LEN \
	MAX(sizeof(CONFIGPREFIX_LAYOUT),\
	MAX(sizeof(CONFIGPREFIX_WIDTH),\
	MAX(sizeof(CONFIGPREFIX_HEIGHT),\
	MAX(sizeof(CONFIGPREFIX_FLAGS),\
	MAX(sizeof(CONFIGPREFIX_WINDOW_TITLE),\
	    sizeof(CONFIGPREFIX_ACTION_TITLE)\
	)))))

#define FLAG_RESIZABLE     0x1
#define FLAG_UPDATE_RESIZE 0x2
#define FLAG_FOCUS         0x4

//An instance is additional data to an action.
//All actions in this plugin should always be attached to a struct instance_s.
struct instance_s{
	GtkWindow *window;
	ddb_gtkui_widget_t *root_container;
	char config_id[WIDGETWINDOWS_ID_MAX_LEN];
	char action_id[WIDGETWINDOWS_ID_MAX_LEN];
	char action_title[100];
	char window_title[100];
	int width;
	int height;
	unsigned char flags; //Flags. See FLAG_*.
	guint toggle_callback_id;
	gulong destroy_handler_id;
	gulong configure_handler_id;
	DB_plugin_action_t action;
};

struct widgetwindows_s{
	struct instance_s *insts;
} widgetwindows;

//Only for the actions in this plugin.
//Okay because they should all be part of the larger struct instance_s.
static inline struct instance_s *action_get_instance(DB_plugin_action_t *ptr){return (struct instance_s*)(((unsigned char*)ptr)-offsetof(struct instance_s,action));}

//Just uses the linked list of the action.
static inline struct instance_s *instance_next(struct instance_s *inst){return inst->action.next? action_get_instance(inst->action.next) : NULL;}

static bool widgetwindows_on_resize(__attribute__((unused)) GtkWidget* self,GdkEventConfigure *event,struct instance_s *inst){
	//gtk_window_get_size(GTK_WINDOW(inst->window),&inst->width,&inst->height);
	if(inst->flags & FLAG_UPDATE_RESIZE){
		inst->width = event->width;
		inst->height = event->height;
	}
	return false;
}

static void instance_save(struct instance_s *inst){
	char buffer[CONFIGPREFIX_MAX_LEN + 1 + sizeof(inst->config_id) + 1];

	snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_WIDTH,inst->config_id);
	deadbeef->conf_set_int(buffer ,inst->width);

	snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_HEIGHT,inst->config_id);
	deadbeef->conf_set_int(buffer,inst->height);

	snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_FLAGS,inst->config_id);
	deadbeef->conf_set_int(buffer,(unsigned char)inst->flags);

	snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_LAYOUT,inst->config_id);
	rootwidget_save(inst->root_container,buffer);

	snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_WINDOW_TITLE,inst->config_id);
	deadbeef->conf_set_str(buffer,inst->window_title);

	snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_ACTION_TITLE,inst->config_id);
	deadbeef->conf_set_str(buffer,inst->action_title);
}

static void widgetwindows_on_window_destroy(__attribute__((unused)) GtkWidget* self,struct instance_s *inst){
	instance_save(inst); //TODO: Should probably be running on the main thread

	for(ddb_gtkui_widget_t *c = inst->root_container->children; c; c = c->next){
		gtkui_plugin->w_remove(inst->root_container,c);
		gtkui_plugin->w_destroy(c);
	}

	gtkui_plugin->w_destroy(inst->root_container);
	inst->root_container = NULL;
	inst->window = NULL;
}

static int widgetwindows_window_create(void *user_data){
	struct instance_s *inst = (struct instance_s*)user_data;

	GtkWidget *main_window = gtkui_plugin->get_mainwin();
	if(!main_window) return G_SOURCE_REMOVE;

	inst->window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
		gtk_window_set_transient_for(GTK_WINDOW(inst->window),GTK_WINDOW(main_window));
		gtk_window_set_default_size(GTK_WINDOW(inst->window),inst->width,inst->height);
		gtk_window_set_title(GTK_WINDOW(inst->window),inst->window_title);
		gtk_window_set_modal(GTK_WINDOW(inst->window),false);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(inst->window),true);
		gtk_window_set_resizable(GTK_WINDOW(inst->window),inst->flags & FLAG_RESIZABLE);
		inst->destroy_handler_id = g_signal_connect(G_OBJECT(inst->window),"destroy",G_CALLBACK(widgetwindows_on_window_destroy),inst);
		inst->configure_handler_id = g_signal_connect(G_OBJECT(inst->window),"configure-event",G_CALLBACK(widgetwindows_on_resize),inst);

		//Root widget.
		char buffer[sizeof(CONFIGPREFIX_LAYOUT) + 1 + sizeof(inst->config_id) + 1];
		snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_LAYOUT,inst->config_id);
		rootwidget_init(&inst->root_container,buffer);
		gtk_container_add(GTK_CONTAINER(inst->window),inst->root_container->widget);

		gtk_widget_show(GTK_WIDGET(inst->window));
    	if(inst->flags & FLAG_FOCUS) gtk_window_present(GTK_WINDOW(inst->window));
	return G_SOURCE_REMOVE;
}

static int widgetwindows_window_destroy(void *user_data){
	struct instance_s *inst = (struct instance_s*)user_data;
	gtk_widget_destroy(GTK_WIDGET(inst->window));
	return G_SOURCE_REMOVE;
}

static void widgetwindows_toggle_end_callback(void *user_data){
	((struct instance_s*)user_data)->toggle_callback_id = 0;
}

static int action_toggle_window_callback(DB_plugin_action_t *action,__attribute__ ((unused)) ddb_action_context_t ctx){
	struct instance_s *inst = action_get_instance(action);
	if(inst->toggle_callback_id == 0){
		g_idle_add_full(G_PRIORITY_LOW,inst->window? widgetwindows_window_destroy : widgetwindows_window_create,inst,widgetwindows_toggle_end_callback);
	}
	return 0;
}

static struct instance_s *instance_add(const char *id){
	struct instance_s *inst = malloc(sizeof(struct instance_s));
	if(!inst) return NULL;

	//Append to the linked list.
	if(!widgetwindows.insts){
		widgetwindows.insts = inst;
	}else{
		if(strncmp(id,widgetwindows.insts->config_id,WIDGETWINDOWS_ID_MAX_LEN) == 0) return NULL;

		DB_plugin_action_t **p = &widgetwindows.insts->action.next;
		while(*p){
			if(strncmp(id,action_get_instance(*p)->config_id,WIDGETWINDOWS_ID_MAX_LEN) == 0) return NULL;
			p = &(*p)->next;
		}
		*p = &inst->action;
	}

	inst->window = NULL;
	inst->root_container = NULL;
	inst->toggle_callback_id = 0;

	snprintf(inst->config_id,sizeof(inst->config_id),"%s",id);
	snprintf(inst->action_id,sizeof(inst->action_id),"widgetwindows_toggle_%s",id);

	{
		char buffer[CONFIGPREFIX_MAX_LEN + 1 + sizeof(inst->config_id) + 1];

		snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_WIDTH,inst->config_id);
		inst->width = deadbeef->conf_get_int(buffer,256);
		if(inst->width < 16) inst->width = 16;

		snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_HEIGHT,inst->config_id);
		inst->height = deadbeef->conf_get_int(buffer,256);
		if(inst->height < 16) inst->height = 16;

		snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_FLAGS,inst->config_id);
		inst->flags = (unsigned char)deadbeef->conf_get_int(buffer,0xff);

		deadbeef->conf_lock();
		{
			snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_ACTION_TITLE,inst->config_id);
			const char *s = deadbeef->conf_get_str_fast(buffer,NULL);
			if(s){snprintf(inst->action_title,sizeof(inst->action_title),"%s",s);}
			else {snprintf(inst->action_title,sizeof(inst->action_title),"View/Window %s",id);}
		}{
			snprintf(buffer,sizeof(buffer),"%s.%s",CONFIGPREFIX_WINDOW_TITLE,inst->config_id);
			const char *s = deadbeef->conf_get_str_fast(buffer,NULL);
			if(s){snprintf(inst->window_title,sizeof(inst->window_title),"%s",s);}
			else {snprintf(inst->window_title,sizeof(inst->window_title),"Window %s",id);}
		}
		deadbeef->conf_unlock();
	}

	inst->action = (DB_plugin_action_t){
		.title = inst->action_title,
		.name = inst->action_id,
		.flags = DB_ACTION_COMMON | DB_ACTION_ADD_MENU,
		.callback2 = action_toggle_window_callback,
		.next = NULL
	};

	return inst;
}

static int widgetwindows_start(){
	widgetwindows.insts = NULL;

	for(int i=0,n=deadbeef->conf_get_int(CONFIG_WINDOW_COUNT,0); i<n; i+=1){
		//TODO: just a temporary solution. maybe use conf_find, see deadbeef/src/playlist.c:pl_load_all or a semicolon separated field in the config
		char buffer[1+20 + 1]; snprintf(buffer,sizeof(buffer),"%05d",i);
		instance_add(buffer);
	}

	return 0;
}

static int widgetwindows_connect(void){
	if(!(gtkui_plugin = (ddb_gtkui_t*) deadbeef->plug_get_for_id(DDB_GTKUI_PLUGIN_ID))){
		return -1;
	}
	//gtkui_plugin->add_window_init_hook(widgetwindows_window_init_hook,NULL);

	return 0;
}

static int widgetwindows_message(uint32_t id,uintptr_t ctx,uint32_t p1,uint32_t p2){
	switch(id){
		case DB_EV_TERMINATE:
			//Save when windows are open and application terminates.
			for(struct instance_s *inst = widgetwindows.insts; inst; inst = instance_next(inst)){
				if(inst->window){
					instance_save(inst);
					g_signal_handler_disconnect(G_OBJECT(inst->window),inst->destroy_handler_id);
					inst->destroy_handler_id = 0;
					g_signal_handler_disconnect(G_OBJECT(inst->window),inst->configure_handler_id);
					inst->configure_handler_id = 0;
				}
			}
			break;
	}

	for(struct instance_s *inst = widgetwindows.insts; inst; inst = instance_next(inst)){
		if(inst->root_container) gtkui_plugin->w_send_message(inst->root_container,id,ctx,p1,p2);
	}
	return 0;
}

static DB_plugin_action_t *widgettoggle_get_actions(__attribute__((unused)) DB_playItem_t *it){
	return widgetwindows.insts? &widgetwindows.insts->action : NULL;
}

static const char settings_dlg[] =
	//TODO: Possible to implement without restart. Only need to reload actions. Fix later.
	"property \"Windows (requires restart)\" spinbtn[0,40,1] " CONFIG_WINDOW_COUNT " 0;"
;

static struct instance_s *api_get_instance(const char *config_id){
	for(struct instance_s *inst = widgetwindows.insts; inst; inst = instance_next(inst)){
		if(strcmp(config_id,inst->config_id) == 0) return inst;
	}
	return NULL;
}
static struct instance_s *api_get_instance_from_rootwidget(ddb_gtkui_widget_t *rootwidget){
	for(struct instance_s *inst = widgetwindows.insts; inst; inst = instance_next(inst)){
		if(inst->root_container == rootwidget) return inst;
	}
	return NULL;
}
static struct instance_s *api_get_instance_by_index(size_t i){
	for(struct instance_s *inst = widgetwindows.insts; inst; inst = instance_next(inst)){
		if(i-- == 0) return inst;
	}
	return NULL;
}
static size_t api_count_instances(){
	size_t out = 0;
	for(struct instance_s *inst = widgetwindows.insts; inst; inst = instance_next(inst)){
		out+= 1;
	}
	return out;
}
static void                api_instance_open            (struct instance_s *inst){if(inst->toggle_callback_id == 0 && !inst->window) inst->toggle_callback_id = g_idle_add_full(G_PRIORITY_LOW,widgetwindows_window_create,inst,widgetwindows_toggle_end_callback);}
static void                api_instance_close           (struct instance_s *inst){if(inst->toggle_callback_id == 0 && inst->window)  inst->toggle_callback_id = g_idle_add_full(G_PRIORITY_LOW,widgetwindows_window_destroy,inst,widgetwindows_toggle_end_callback);}
static void                api_instance_toggle          (struct instance_s *inst){if(inst->toggle_callback_id == 0) inst->toggle_callback_id = g_idle_add_full(G_PRIORITY_LOW,inst->window? widgetwindows_window_destroy : widgetwindows_window_create,inst,widgetwindows_toggle_end_callback);}
static bool                api_instance_is_visible      (struct instance_s *inst){return inst->window != NULL;}
static GtkWindow          *api_instance_get_window      (struct instance_s *inst){return inst->window;}
static ddb_gtkui_widget_t *api_instance_get_rootwidget  (struct instance_s *inst){return inst->root_container;}
static const char         *api_instance_get_config_id   (struct instance_s *inst){return inst->config_id;}
static const char         *api_instance_get_action_id   (struct instance_s *inst){return inst->action_id;}
static const char         *api_instance_get_action_title(struct instance_s *inst){return inst->action_title;}
static const char         *api_instance_get_window_title(struct instance_s *inst){return inst->window_title;}
static const char widgetwindows_license[] = {
#embed "../LICENSE"
,'\0'};
static ddb_widgetwindows_t plugin ={
	.misc.plugin.api_vmajor = DB_API_VERSION_MAJOR,
	.misc.plugin.api_vminor = DB_API_VERSION_MINOR,
	.misc.plugin.version_major = 1,
	.misc.plugin.version_minor = 0,
	.misc.plugin.type = DB_PLUGIN_MISC,
	#if GTK_CHECK_VERSION(3,0,0)
	.misc.plugin.id = "widgetwindows_gtk3",
	#else
	.misc.plugin.id = "widgetwindows_gtk2",
	#endif
	.misc.plugin.name = "Widget Windows",
	.misc.plugin.descr =
		"Customisable widget windows.\n"
		"\n"
		"Adds windows that can be toggled by actions.\n"
		"The windows will contain a root widget that can be modified in Design Mode,\n"
		"the same way as widgets in the main window.\n"
		"The root widgets are saved in the configuration in the same way as the main window root widget is.\n"
		"\n"
		"Instructions:\n"
		"Set the number of windows in the plugin configuration. Restart for changes.\n"
		"Open a window by using the actions found in \"View/Window *\" (widgetwindows_toggle_*).\n"
		"In the new window, widgets can be added and modified in Design Mode.\n"
		"To save the layout changes made in a window, the window must be closed while in Design Mode.\n"
		"\n"
		"This plugin is not finished as of writing, in particular regarding the GUI configuration (instead, modify in the config file), but should be functional otherwise.\n"
	,
	.misc.plugin.copyright = widgetwindows_license,
	.misc.plugin.website = "https://github.org/EDT4/ddb_widgetwindows",
	.misc.plugin.connect = widgetwindows_connect,
	.misc.plugin.start   = widgetwindows_start,
	.misc.plugin.message = widgetwindows_message,
	.misc.plugin.get_actions = &widgettoggle_get_actions,
	.misc.plugin.configdialog = settings_dlg,
	.get_instance                 = api_get_instance,
	.get_instance_from_rootwidget = api_get_instance_from_rootwidget,
	.get_instance_by_index        = api_get_instance_by_index,
	.count_instances              = api_count_instances,
	.instance_open                = api_instance_open,
	.instance_close               = api_instance_close,
	.instance_toggle              = api_instance_toggle,
	.instance_is_visible          = api_instance_is_visible,
	.instance_get_window          = api_instance_get_window,
	.instance_get_rootwidget      = api_instance_get_rootwidget,
	.instance_get_config_id       = api_instance_get_config_id,
	.instance_get_action_id       = api_instance_get_action_id,
	.instance_get_action_title    = api_instance_get_action_title,
	.instance_get_window_title    = api_instance_get_window_title,
	.instance_add                 = instance_add,
};

__attribute__((visibility("default")))
DB_plugin_t *
#if GTK_CHECK_VERSION(3,0,0)
widgetwindows_gtk3_load
#else
widgetwindows_gtk2_load
#endif
(DB_functions_t *api){
	deadbeef = api;
	return DB_PLUGIN(&plugin);
}
