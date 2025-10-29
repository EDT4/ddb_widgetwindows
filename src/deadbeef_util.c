#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>

extern DB_functions_t *deadbeef;
extern ddb_gtkui_t *gtkui_plugin;

void rootwidget_init(ddb_gtkui_widget_t **container,const char *conf_field){
	*container = gtkui_plugin->w_create("box");
	gtk_widget_show((*container)->widget);

	ddb_gtkui_widget_t *w = gtkui_plugin->conf_get_widget(conf_field);
	if(!w){w = gtkui_plugin->w_create("placeholder");}
	gtkui_plugin->w_append(*container,w);
}

void rootwidget_save(ddb_gtkui_widget_t *container,const char *conf_field){
	if(!container || !container->children) return;
	gtkui_plugin->conf_set_widget(conf_field,container->children);
}
