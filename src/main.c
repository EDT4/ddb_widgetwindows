#include <gtk/gtk.h>
#include <jansson.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>
#include <stdbool.h>
#include "api.h"
#include "deadbeef_util.h"

DB_functions_t *deadbeef;
ddb_gtkui_t *gtkui_plugin;

#define WIDGETDIALOG_CONFIG_LAYOUT "widgetdialog.layout"

struct widgetdialog_t{
	GtkDialog *dialog;
	ddb_gtkui_widget_t *root_container;
	int width;
	int height;
} widgetdialog;

static void widgetdialog_root_widget_init(ddb_gtkui_widget_t **container,const char *conf_field){
	*container = gtkui_plugin->w_create("box");
	gtk_widget_show((*container)->widget);

	ddb_gtkui_widget_t *w = NULL;
	deadbeef->conf_lock();
	const char *json = deadbeef->conf_get_str_fast(conf_field,NULL);
	if(json){
		json_t *layout = json_loads(json,0,NULL);
		if(layout != NULL){
			if(w_create_from_json(layout,&w) >= 0){
				gtkui_plugin->w_append(*container,w);
			}
			json_delete(layout);
		}
	}
	deadbeef->conf_unlock();

	if(!w){
		w = gtkui_plugin->w_create("placeholder");
		gtkui_plugin->w_append(*container,w);
	}
}

static void widgetdialog_root_widget_save(ddb_gtkui_widget_t *container,const char *conf_field){
	if(!container || !container->children) return;

	json_t *layout = w_save_widget_to_json(container->children);
	if(layout){
		char *layout_str = json_dumps(layout,JSON_COMPACT);
		if(layout_str){
			deadbeef->conf_set_str(conf_field,layout_str);
			free(layout_str);
		}
		json_delete(layout);
	}
}


void widgetdialog_on_resize(GtkDialog* self,gpointer user_data){
	gtk_window_get_size(GTK_WINDOW(widgetdialog.dialog),&widgetdialog.width,&widgetdialog.height);
}

void widgetdialog_on_dialog_destroy(GtkDialog* self,gpointer user_data){
	for(ddb_gtkui_widget_t *c = widgetdialog.root_container->children; c; c = c->next){
		gtkui_plugin->w_remove(widgetdialog.root_container,c);
		gtkui_plugin->w_destroy(c);
	}

	gtkui_plugin->w_destroy(widgetdialog.root_container);
	widgetdialog.root_container = NULL;
	widgetdialog.dialog = NULL;
}

static int widgetdialog_dialog_create(__attribute__((unused)) void *user_data){
	widgetdialog.dialog = GTK_DIALOG(gtk_dialog_new());
		gtk_window_set_default_size(GTK_WINDOW(widgetdialog.dialog),widgetdialog.width,widgetdialog.height);
		gtk_window_set_title(GTK_WINDOW(widgetdialog.dialog),"Testing");
		gtk_window_set_modal(GTK_WINDOW(widgetdialog.dialog),false);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(widgetdialog.dialog),true);
		g_signal_connect(G_OBJECT(widgetdialog.dialog),"destroy",G_CALLBACK(widgetdialog_on_dialog_destroy),NULL);
		g_signal_connect(G_OBJECT(widgetdialog.dialog),"configure-event",G_CALLBACK(widgetdialog_on_resize),NULL);

		//Root widget.
		widgetdialog_root_widget_init(&widgetdialog.root_container,WIDGETDIALOG_CONFIG_LAYOUT);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(widgetdialog.dialog)),widgetdialog.root_container->widget,true,true,0);

		gtk_widget_show(GTK_WIDGET(widgetdialog.dialog));
	return G_SOURCE_REMOVE;
}

static int widgetdialog_dialog_destroy(__attribute__((unused)) void *user_data){
	gtk_widget_destroy(GTK_WIDGET(widgetdialog.dialog));
	return G_SOURCE_REMOVE;
}

static int widgetdialog_start(){
	widgetdialog.dialog = NULL;
	widgetdialog.root_container = NULL;
	widgetdialog.width = deadbeef->conf_get_int("widgetdialog.window.width",256);
	if(widgetdialog.width < 16) widgetdialog.width = 16;
	widgetdialog.height = deadbeef->conf_get_int("widgetdialog.window.height",256);
	if(widgetdialog.height < 16) widgetdialog.height = 16;
	return 0;
}

static int widgetdialog_stop(){
	deadbeef->conf_set_int("widgetdialog.window.width",widgetdialog.width);
	deadbeef->conf_set_int("widgetdialog.window.height",widgetdialog.height);
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
			widgetdialog_root_widget_save(widgetdialog.root_container,WIDGETDIALOG_CONFIG_LAYOUT);
			break;
	}
	if(widgetdialog.root_container) send_messages_to_widgets(widgetdialog.root_container,id,ctx,p1,p2);
	return 0;
}

static int action_toggle_dialog_callback(__attribute__ ((unused)) DB_plugin_action_t *action,ddb_action_context_t ctx){
	g_idle_add(widgetdialog.dialog? widgetdialog_dialog_destroy : widgetdialog_dialog_create,NULL);
	return 0;
}
static DB_plugin_action_t action_toggle_dialog = {
	.title = "Toggle dialog",
	.name = "widgetdialog_toggle",
	.flags = DB_ACTION_COMMON | DB_ACTION_ADD_MENU,
	.callback2 = action_toggle_dialog_callback,
	.next = NULL
};
static DB_plugin_action_t *widgettoggle_get_actions(DB_playItem_t *it){
    return &action_toggle_dialog;
}

static GtkDialog          *api_get_dialog(){return widgetdialog.dialog;}
static ddb_gtkui_widget_t *api_get_rootwidget(){return widgetdialog.root_container;}
static ddb_widgetdialog_t plugin ={
	.misc.plugin.api_vmajor = DB_API_VERSION_MAJOR,
	.misc.plugin.api_vminor = DB_API_VERSION_MINOR,
	.misc.plugin.version_major = 1,
	.misc.plugin.version_minor = 0,
	.misc.plugin.type = DB_PLUGIN_MISC,
	.misc.plugin.id = "widgetdialog-gtk3",
	.misc.plugin.name = "Widget Dialog",
	.misc.plugin.descr =
		"A customisable widget dialog.\n"
		"Create the dialog by the \"Toggle dialog\" action (widgetdialog_toggle).\n"
		"In the new dialog, widgets can be added and modified in Design Mode.",
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
	.misc.plugin.stop    = widgetdialog_stop,
	.misc.plugin.message = widgetdialog_message,
	.misc.plugin.get_actions = &widgettoggle_get_actions,
	.get_dialog     = api_get_dialog,
	.get_rootwidget = api_get_rootwidget,
};

__attribute__((visibility("default")))
DB_plugin_t * widgetdialog_gtk3_load(DB_functions_t *api){
	deadbeef = api;
	return DB_PLUGIN(&plugin);
}
