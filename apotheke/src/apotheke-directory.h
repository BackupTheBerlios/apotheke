#ifndef __APOTHEKE_DIRECTORY_H__
#define __APOTHEKE_DIRECTORY_H__

#include <gtk/gtkliststore.h>

#include "apotheke-file.h"

#define APOTHEKE_TYPE_DIRECTORY	    (apotheke_directory_get_type ())
#define APOTHEKE_DIRECTORY(obj)	    (GTK_CHECK_CAST ((obj), APOTHEKE_TYPE_DIRECTORY, ApothekeDirectory))
#define APOTHEKE_DIRECTORY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), APOTHEKE_TYPE_DIRECTORY, ApothekeDirectoryClass))
#define APOTHEKE_IS_DIRECTORY(obj)	   (GTK_CHECK_TYPE ((obj), APOTHEKE_TYPE_DIRECTORY))
#define APOTHEKE_IS_DIRECTORY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), APOTHEKE_TYPE_DIRECTORY))

typedef struct _ApothekeDirectory ApothekeDirectory;
typedef struct _ApothekeDirectoryClass ApothekeDirectoryClass;
typedef struct _ApothekeDirectoryPrivate ApothekeDirectoryPrivate;

enum {
	AD_COL_FLAG,
	AD_COL_FILEICON,
	AD_COL_FILENAME,
	AD_COL_VERSION,
	AD_COL_STATUS,
	AD_COL_ATTRIBUTES,
	AD_COL_TAG,
	AD_COL_DATE,
	AD_COL_FILESTRUCT,
	AD_NUM_COLUMNS
};

struct _ApothekeDirectory {
	GtkListStore     parent_object;

	ApothekeDirectoryPrivate *priv;
};

struct _ApothekeDirectoryClass {
	GtkListStoreClass  parent_class;
};

GType apotheke_directory_get_type (void);

ApothekeDirectory* apotheke_directory_new (const gchar *dir);

void apotheke_directory_create_file_list (ApothekeDirectory *dir);

gchar* apotheke_directory_get_uri (ApothekeDirectory *dir);

void apotheke_directory_set_hide_ignored_files (ApothekeDirectory *dir, 
						gboolean hide);

void apotheke_directory_dump (ApothekeDirectory *dir);

#endif /* __APOTHEKE_DIRECTORY_H__ */
