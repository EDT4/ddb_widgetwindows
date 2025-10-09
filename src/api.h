#ifndef __DDB_CUSTOMHEADERBAR_API_H
#define __DDB_CUSTOMHEADERBAR_API_H

typedef struct ddb_widgetdialog_s{
	DB_misc_t misc;
    GtkDialog          *(*get_dialog)();
    ddb_gtkui_widget_t *(*get_rootwidget)();
} ddb_widgetdialog_t;

#endif
