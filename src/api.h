#ifndef __DDB_WIDGETWINDOWS_API_H
#define __DDB_WIDGETWINDOWS_API_H

struct instance_s;
struct ddb_gtkui_widget_s;

#define WIDGETWINDOWS_ID_MAX_LEN 50

typedef struct ddb_widgetwindows_s{
	DB_misc_t misc;
	struct instance_s  *(*get_instance)                (const char *config_id);
	struct instance_s  *(*get_instance_from_rootwidget)(struct ddb_gtkui_widget_s *rootwidget);
	struct instance_s  *(*get_instance_by_index)       (size_t i);
	size_t              (*count_instances)             ();
	void                (*instance_open)               (struct instance_s *inst);
	void                (*instance_close)              (struct instance_s *inst);
	void                (*instance_toggle)             (struct instance_s *inst);
	bool                (*instance_is_visible)         (struct instance_s *inst);
	GtkWindow          *(*instance_get_window)         (struct instance_s *inst);
	ddb_gtkui_widget_t *(*instance_get_rootwidget)     (struct instance_s *inst);
	const char         *(*instance_get_config_id)      (struct instance_s *inst);
	const char         *(*instance_get_action_id)      (struct instance_s *inst);
	const char         *(*instance_get_action_title)   (struct instance_s *inst);
	const char         *(*instance_get_window_title)   (struct instance_s *inst);
	struct instance_s  *(*instance_add)                (const char *config_id);
} ddb_widgetwindows_t;

#endif
