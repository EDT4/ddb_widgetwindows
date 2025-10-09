#ifndef __DDB_CUSTOMHEADERBAR_API_H
#define __DDB_CUSTOMHEADERBAR_API_H

struct instance_s;

typedef struct ddb_widgetdialog_s{
	DB_misc_t misc;
	GtkDialog          *(*instance_get_dialog)    (struct instance_s *inst);
	ddb_gtkui_widget_t *(*instance_get_rootwidget)(struct instance_s *inst);
} ddb_widgetdialog_t;

#endif



