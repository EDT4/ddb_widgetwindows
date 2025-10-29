#ifndef __DDB_WIDGETWINDOWS_DEADBEEF_UTIL_H
#define __DDB_WIDGETWINDOWS_DEADBEEF_UTIL_H

struct ddb_gtkui_widget_s;
struct json_t;

void rootwidget_init(struct ddb_gtkui_widget_s **container,const char *conf_field);
void rootwidget_save(struct ddb_gtkui_widget_s *container,const char *conf_field);

#endif
