#ifndef __APOTHEKE_FILE_H__
#define __APOTHEKE_FILE_H__

#include <time.h>
#include <glib.h>

typedef struct _ApothekeFile ApothekeFile;

typedef enum {
	FILE_TYPE_UNKNOWN,
	FILE_TYPE_NOT_IN_CVS,
	FILE_TYPE_IGNORE,
	FILE_TYPE_CVS
} ApothekeFileType;

typedef enum {
	FILE_STATUS_NONE,
	FILE_STATUS_UP_TO_DATE,
	FILE_STATUS_NEEDS_PATCH,
	FILE_STATUS_ADDED,
	FILE_STATUS_MODIFIED,
	FILE_STATUS_REMOVED,
	FILE_STATUS_NEEDS_CHECKOUT,
	FILE_STATUS_NEEDS_MERGE,
	FILE_STATUS_MISSING,
	FILE_STATUS_CONFLICT
} ApothekeFileStatus;

struct _ApothekeFile {
	GQuark              quark;
	gchar               *filename;
	gboolean            directory;
	time_t              mtime;

	ApothekeFileType    type;
	ApothekeFileStatus  status;
	gchar               *cvs_revision; 
	gchar               *cvs_date;
	gchar               *cvs_option;
	gchar               *cvs_tag;
};

#endif /* __APOTHEKE_FILE_H__ */
