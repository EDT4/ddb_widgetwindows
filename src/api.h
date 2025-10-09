#ifndef __DDB_CUSTOMHEADERBAR_API_H
#define __DDB_CUSTOMHEADERBAR_API_H

struct instance_s;

typedef struct ddb_widgetdialog_s{
	DB_misc_t misc;
	struct instance_s  *(*get_instance)         (const char *config_id);
	GtkDialog          *(*instance_get_dialog)      (struct instance_s *inst);
	ddb_gtkui_widget_t *(*instance_get_rootwidget)  (struct instance_s *inst);
	const char         *(*instance_get_config_id)   (struct instance_s *inst);
	const char         *(*instance_get_action_id)   (struct instance_s *inst);
	const char         *(*instance_get_action_title)(struct instance_s *inst);
	const char         *(*instance_get_dialog_title)(struct instance_s *inst);
} ddb_widgetdialog_t;

#endif



