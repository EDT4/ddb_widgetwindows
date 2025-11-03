#ifndef __DDB_WIDGETWINDOWS_API_H
#define __DDB_WIDGETWINDOWS_API_H

struct instance_s;
struct ddb_gtkui_widget_s;

typedef struct ddb_widgetwindows_s{
	DB_misc_t misc;
	struct instance_s  *(*get_instance)                (const char *config_id);
	struct instance_s  *(*get_instance_from_rootwidget)(struct ddb_gtkui_widget_s *rootwidget);
	struct instance_s  *(*get_instance_by_index)       (size_t i);
	size_t              (*count_instances)             ();
	GtkWindow          *(*instance_get_window)         (struct instance_s *inst);
	ddb_gtkui_widget_t *(*instance_get_rootwidget)     (struct instance_s *inst);
	const char         *(*instance_get_config_id)      (struct instance_s *inst);
	const char         *(*instance_get_action_id)      (struct instance_s *inst);
	const char         *(*instance_get_action_title)   (struct instance_s *inst);
	const char         *(*instance_get_window_title)   (struct instance_s *inst);
} ddb_widgetwindows_t;

#endif
