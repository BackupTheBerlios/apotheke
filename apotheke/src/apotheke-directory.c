#include <config.h>

#include <stdio.h>
#include <time.h>

#include <glib/gstrfuncs.h>
#include <glib/gquark.h>
#include <glib/gdataset.h>
#include <glib/gmessages.h>
#include <glib/gpattern.h>
#include <glib/glist.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-directory.h>
#include <libgnome/gnome-macros.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-i18n.h>

#include "apotheke-directory.h"
#include "apotheke-cvs-entries.h"

#define LIST_VIEW_ICON_HEIGHT           24

struct _ApothekeDirectoryPrivate {
	gchar     *uri;
	gboolean  hide_ignored_files;

#ifdef HAVE_LIBFAM
	GnomeVFSMonitorHandle *monitor;
#endif
};

static void apotheke_directory_instance_init (ApothekeDirectory *dir);
static void apotheke_directory_class_init (ApothekeDirectoryClass *klass);
static void apply_cvs_status (ApothekeDirectory *ad, GtkTreeIter *iter, 
			      time_t mtime, GList **list);
static gboolean is_file_ignored (gchar *filename, GList *pattern_list);
static void get_status_text (ApothekeFileStatus status, 
			     gboolean is_directory, 
			     gchar **status_str);
static GdkPixbuf* get_file_icon (ApothekeFileStatus status, 
				 gboolean is_directory);

static void apotheke_directory_finalize (GObject *object);
static void apotheke_directory_dispose (GObject *object);
static gint apotheke_sort_func (GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer user_data);

GNOME_CLASS_BOILERPLATE (ApothekeDirectory, apotheke_directory,
			 GtkListStore, GTK_TYPE_LIST_STORE)

static void 
apotheke_directory_instance_init (ApothekeDirectory *dir)
{
	ApothekeDirectoryPrivate *priv;

	priv = g_new0 (ApothekeDirectoryPrivate, 1);
	
	dir->priv = priv;
	priv->uri = NULL;
	priv->hide_ignored_files = FALSE;
#ifdef HAVE_LIBFAM
	priv->monitor = NULL;
#endif
}

static void 
apotheke_directory_class_init (ApothekeDirectoryClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = apotheke_directory_dispose;
        gobject_class->finalize = apotheke_directory_finalize;
}

static void
apotheke_directory_construct (ApothekeDirectory *ad, const gchar *uri)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *info;

	GType column_types [AD_NUM_COLUMNS] = {
		GDK_TYPE_PIXBUF, /* AD_COL_FILEICON */
		G_TYPE_BOOLEAN,  /* AD_COL_DIRECTORY */
		G_TYPE_STRING,   /* AD_COL_FILENAME */ 
		G_TYPE_STRING,   /* AD_COL_VERSION */
		G_TYPE_INT,      /* AD_COL_STATUS */
		G_TYPE_STRING,   /* AD_COL_STATUS_STR */
		G_TYPE_STRING,   /* AD_COL_ATTRIBUTES */
		G_TYPE_STRING,   /* AD_COL_TAG */
		G_TYPE_STRING,   /* AD_COL_DATE */
		G_TYPE_LONG,     /* AD_COL_MTIME */
	};

	gtk_list_store_set_column_types  (GTK_LIST_STORE (ad),
					  AD_NUM_COLUMNS, 
					  column_types);

	if (uri == NULL) return;
	
	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (uri, info, GNOME_VFS_FILE_INFO_DEFAULT);
	if (result == GNOME_VFS_OK && info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		ad->priv->uri = g_strdup (uri);
	}
	else {
		g_print (_("Couldn't open %s as directory.\n"), uri);
		ad->priv->uri = NULL;
	}
	gnome_vfs_file_info_unref (info);

	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (ad),
					 AD_COL_FILENAME, 
					 apotheke_sort_func, NULL, NULL);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (ad),
					      AD_COL_FILENAME, GTK_SORT_ASCENDING);
}

ApothekeDirectory* 
apotheke_directory_new (const gchar *uri)
{
	ApothekeDirectory *ad;

	ad = g_object_new (APOTHEKE_TYPE_DIRECTORY, NULL);
	
	apotheke_directory_construct (ad, uri);

	return ad;
}

static void 
apotheke_directory_dispose (GObject *object)
{
	ApothekeDirectory *ad;

	ad = APOTHEKE_DIRECTORY (object);

#ifdef HAVE_LIBFAM
	if (ad->priv->monitor)
		gnome_vfs_monitor_cancel (ad->priv->monitor);
	ad->priv->monitor = NULL;
#endif
}

static void 
apotheke_directory_finalize (GObject *object)
{
	ApothekeDirectory *ad;

	ad = APOTHEKE_DIRECTORY (object);

	if (ad->priv->uri)
		g_free (ad->priv->uri);
	ad->priv->uri = NULL;

	g_free (ad->priv);
}


static gint 
apotheke_sort_func (GtkTreeModel *model,
		    GtkTreeIter *a,
		    GtkTreeIter *b,
		    gpointer user_data)
{
	gboolean a_is_directory;
	gboolean b_is_directory;
	char *a_name;
	char *b_name;

	gtk_tree_model_get (model, a, 
			    AD_COL_FILENAME, &a_name, 
			    AD_COL_DIRECTORY, &a_is_directory,
			    -1);
	gtk_tree_model_get (model, b, 
			    AD_COL_FILENAME, &b_name, 
			    AD_COL_DIRECTORY, &b_is_directory,
			    -1);

	if (a_is_directory == b_is_directory) {
		return g_utf8_collate (a_name, b_name);
	}
	else if (a_is_directory) {
		return -1;
	}
	else {
		return 1;
	}
}

#ifdef HAVE_LIBFAM
static void
monitor_callback (GnomeVFSMonitorHandle *handle,
		  const gchar *monitor_uri,
		  const gchar *info_uri,
		  GnomeVFSMonitorEventType event_type,
		  gpointer user_data)
{
#if 0
	ApothekeDirectory *ad;
	ApothekeFile *af;

	ad = APOTHEKE_DIRECTORY (user_data);

	switch (event_type) {
	case GNOME_VFS_MONITOR_EVENT_CHANGED:
		g_print ("Changed: %s  %s\n", monitor_uri, info_uri);
		/* FIXME: these functions do not exsist yet. */
		af = get_apotheke_file_from_uri (ad, info_uri);
		if (af->type == FILE_TYPE_CVS) {
			af->status = FILE_STATUS_MODIFIED;
			apply_apotheke_file_to_model (ad, af);
		}
		break;
	case GNOME_VFS_MONITOR_EVENT_DELETED:
		g_print ("Deleted: %s  %s\n", monitor_uri, info_uri);
		break;
	case GNOME_VFS_MONITOR_EVENT_CREATED:
		g_print ("Created: %s  %s\n", monitor_uri, info_uri);
		break;
	default:
	}
#endif
}
#endif

static void
create_file_list (ApothekeDirectory *ad, GList **cvs_entries, 
		  gboolean hide_ignored, GList *pattern_list)
{
	ApothekeDirectoryPrivate *priv;
	GnomeVFSResult result;
	GList *dir_list;
	GList *it;
	
	g_return_if_fail (APOTHEKE_IS_DIRECTORY (ad));
	
	priv = ad->priv;

	if (priv->uri == NULL) return;
	
	/* obtain files in directory */
	dir_list = NULL;
	result = gnome_vfs_directory_list_load (&dir_list, priv->uri, 
						GNOME_VFS_FILE_INFO_DEFAULT);
	if (result != GNOME_VFS_OK) {
		g_warning (_("Couldn't open directory %s: %s"),
			   priv->uri,
			   gnome_vfs_result_to_string (result));
		return;
	}
	
	/* filter ignored files and determien cvs status */
	for (it = dir_list; it != NULL; it = it->next) {
		GnomeVFSFileInfo *info;
		GtkTreeIter iter;
		gboolean is_directory;
		char *filename;
		char *date;
		char *status_str;
		ApothekeFileStatus status = FILE_STATUS_NOT_IN_CVS;
		gboolean ignored;
		
		info = (GnomeVFSFileInfo*) it->data;
		
		if (g_strcasecmp (info->name, "..") != 0 &&
		    g_strcasecmp (info->name, ".") != 0 &&
		    g_strcasecmp (info->name, "CVS") != 0) 
		{
			is_directory = (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY);
			filename = g_filename_to_utf8 (info->name, -1, NULL, NULL, NULL);
		
			ignored = is_file_ignored (filename, pattern_list);
			if (!ignored || (ignored && !hide_ignored)) {
				if (ignored) {
					status = FILE_STATUS_IGNORE;
				}
				get_status_text (status, is_directory, &status_str);
				
				/* at this time the file will be shown in the list */ 
				/* add first information we have here */
				gtk_list_store_append (GTK_LIST_STORE (ad), &iter);
				gtk_list_store_set (GTK_LIST_STORE (ad), &iter,
						    AD_COL_DIRECTORY, is_directory,
						    AD_COL_FILENAME, filename,
						    AD_COL_STATUS, (int) status,
						    AD_COL_STATUS_STR, status_str,
						    -1);
				
				apply_cvs_status (ad, &iter, info->mtime, cvs_entries);
			
			}
			g_free (filename);

		}
		gnome_vfs_file_info_unref (info);
	}

	g_list_free (dir_list);

	/* Add all remaining elements in the cvs_entries list. These are the files
	 * which are in CVS but not in the local filesystem (status = missing).
	 */
	for (it = *cvs_entries; it != 0; it = it->next) {
		char **entry = (char**) it->data;
		gboolean is_directory;
		char *filename;
		GtkTreeIter iter;
		ApothekeFileStatus status = FILE_STATUS_MISSING;
		char *status_str;

		is_directory = (entry[CVS_ENTRIES_DIR][0] == 'D');
		filename = g_filename_to_utf8 (entry[CVS_ENTRIES_FILENAME], -1, NULL, NULL, NULL);
		get_status_text (status, is_directory, &status_str);

		gtk_list_store_append (GTK_LIST_STORE (ad), &iter);
		gtk_list_store_set (GTK_LIST_STORE (ad), &iter,
				    AD_COL_FILEICON, get_file_icon (status, is_directory), 
				    AD_COL_DIRECTORY, is_directory,
				    AD_COL_FILENAME, filename,
				    AD_COL_STATUS, (int) status,
				    AD_COL_STATUS_STR, status_str, 
				    -1);
	}
}

static char**
find_cvs_entry (GList **list, char *filename)
{
	GList *it;

	for (it = *list; it != 0; it = it->next) {
		char **entry = (char**) it->data;
		if (g_strcasecmp (entry[CVS_ENTRIES_FILENAME], filename) == 0) {
			*list = g_list_delete_link (*list, it);
			return entry;
		}
	}

	return 0;
}

static void
apply_cvs_status (ApothekeDirectory *ad, GtkTreeIter *iter, time_t mtime, GList **list)
{
	char **entry;
	char *filename;
	time_t  cvs_time_t = 0;
	struct tm tm_entry;
	ApothekeFileStatus status;
	gboolean is_directory;
	char *cvs_revision = "";
	char *cvs_date = "";
	char *cvs_option = "";
	char *cvs_tag = "";
	char *cvs_status = "";

	gtk_tree_model_get (GTK_TREE_MODEL (ad), iter,
			    AD_COL_DIRECTORY, &is_directory,
			    AD_COL_FILENAME, &filename,
			    AD_COL_STATUS, &status,
			    -1);
	if (filename == 0) return;

	entry = find_cvs_entry (list, filename);
	if (entry == 0) return;

	/* To make the code clearer we have two steps here: First we check
	 * which status the files has. Then on base of the status we fill in
	 * the rest of the information.
	 */
	if (entry[CVS_ENTRIES_DIR][0] == 'D') {
		if (is_directory) {
			status = FILE_STATUS_UP_TO_DATE;
		}
		else {
			status = FILE_STATUS_NOT_IN_CVS;
			g_warning (_("Oops, directory %s has no D flag in CVS/Entries."),
				   filename);
		}
	}
	else if (status == FILE_STATUS_NOT_IN_CVS) {
		/* NOTE: CVS stores UTC time, not localtime. */
		strptime (entry[CVS_ENTRIES_DATE], "%a %h %e %T %Y", &tm_entry);
		cvs_time_t = timegm (&tm_entry);
		
		if (mtime != cvs_time_t) {
			if (entry[CVS_ENTRIES_REVISION][0] == '0' /*NOT '\0'*/) {
				status = FILE_STATUS_ADDED;
			}
			else {
				status = FILE_STATUS_MODIFIED;
			}
		}
		else {
			status = FILE_STATUS_UP_TO_DATE;
		}		

		
	}
	
	/* Second step: fill the rest of the attributes. */

	/* set revision attribute */
	if (entry[CVS_ENTRIES_REVISION][0] != '\0' &&
	    (status == FILE_STATUS_MODIFIED || 
	     status == FILE_STATUS_UP_TO_DATE))
	{
		cvs_revision = entry[CVS_ENTRIES_REVISION];
	}
	else if (entry[CVS_ENTRIES_REVISION][0] != '\0' &&
		 status == FILE_STATUS_REMOVED) 
	{
		cvs_revision = (entry[CVS_ENTRIES_REVISION]+1);
	}
	
	/* set date attribute */
	if (entry[CVS_ENTRIES_DATE][0] != '\0') {
		if (status == FILE_STATUS_MODIFIED ||
		    status == FILE_STATUS_UP_TO_DATE)
		{
			cvs_date = ctime (&cvs_time_t);
		}
		else if (status == FILE_STATUS_ADDED) {
			cvs_date = _("Dummy Timestamp");
		}
	}
	
	/* set option attribute. */
	if (entry[CVS_ENTRIES_OPTION][0] != '\0') {
		cvs_option = entry[CVS_ENTRIES_OPTION];
	}
	
	/* set tag */
	if (entry[CVS_ENTRIES_TAG][0] != '\0') {
		cvs_tag = g_strstrip (entry[CVS_ENTRIES_TAG]+1);
	}

	/* update model */
	get_status_text (status, is_directory, &cvs_status);
	gtk_list_store_set (GTK_LIST_STORE (ad), iter,
			    AD_COL_FILEICON, get_file_icon (status, is_directory),
			    AD_COL_STATUS, (int) status,
			    AD_COL_VERSION, cvs_revision,
			    AD_COL_STATUS_STR, cvs_status,
			    AD_COL_ATTRIBUTES, cvs_option,
			    AD_COL_TAG, cvs_tag,
			    AD_COL_DATE, cvs_date,
			    AD_COL_MTIME, (long) cvs_time_t,
			    -1);

	/* clean up */
	g_strfreev (entry);
}


static gboolean
is_file_ignored (gchar *filename, GList *pattern_list)
{
	GList *it;

	if (pattern_list == 0) return FALSE;

	for (it = pattern_list; it != NULL; it = it->next) 
	{
		if (g_pattern_match_string ((GPatternSpec*)it->data, filename)) {
			g_print ("file ignored: %s\n", filename);
			return TRUE;
		}
	}
}

static GList*
apotheke_directory_get_ignore_pattern_list (ApothekeDirectory *dir)
{
	ApothekeDirectoryPrivate *priv;
	FILE *file;
	gchar *ignore_file;
	gchar *ignore_uri;
	gchar *buffer;
	gchar *utf8;
	gchar *stripped;
	GPatternSpec *pattern;
	GList *pattern_list = NULL;
	GList *it;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;

	g_return_if_fail (APOTHEKE_IS_DIRECTORY (dir));

	priv = dir->priv;

	ignore_uri = g_build_filename (priv->uri, ".cvsignore", NULL);
	info = gnome_vfs_file_info_new ();
	g_print ("ignore uri: %s\n", ignore_uri);
	result = gnome_vfs_get_file_info (ignore_uri, info, GNOME_VFS_FILE_INFO_DEFAULT);
	if (result != GNOME_VFS_OK || info->type != GNOME_VFS_FILE_TYPE_REGULAR) {
		g_print (_("No .cvsignore file found.\n"));
		g_free (ignore_uri); 
		return;
	}
	gnome_vfs_file_info_unref (info);

	ignore_file = gnome_vfs_get_local_path_from_uri (ignore_uri);
	g_print ("ignore_file: %s\n", ignore_file);
	if (ignore_file == NULL) {
		g_free (ignore_uri);
		return;
	}

	file = fopen (ignore_file, "r");
	buffer = g_new0 (gchar, 256);
	
	while (fgets (buffer, 256, file)) {
		stripped = g_strdup (g_strstrip (buffer));
		utf8 = g_convert (stripped, -1, "Latin1", "UTF-8", NULL, NULL, NULL);
		g_print ("ignore pattern: %s\n", utf8);
		if (utf8 != 0) {
			pattern = g_pattern_spec_new (utf8);
			pattern_list = g_list_append (pattern_list, pattern);
			g_free (utf8);
		}

		if (stripped != 0)
			g_free (stripped);
	}

	fclose (file);
	g_free (buffer);
	g_free (ignore_file);
	g_free (ignore_uri);


	return pattern_list;
}

static void
get_status_text (ApothekeFileStatus status, gboolean is_directory, gchar **status_str)
{
	*status_str = "";

	if (is_directory) {
		if (status == FILE_STATUS_NOT_IN_CVS) {
			*status_str = _("Not in CVS");
		}
		else {
			*status_str = _("Directory");
		}
	}
	else {
		switch (status) {
		case FILE_STATUS_NOT_IN_CVS:
			*status_str = _("Not in CVS");
			break;
		case FILE_STATUS_IGNORE:
			*status_str = _("Ignore");
			break;
		case FILE_STATUS_UP_TO_DATE:
			*status_str = _("Up-To-Date");
			break;
		case FILE_STATUS_NEEDS_PATCH:
			*status_str = _("Needs Patch");
			break;
		case FILE_STATUS_ADDED:
			*status_str = _("Added");
			break;
		case FILE_STATUS_MODIFIED:
			*status_str = _("Modified");
			break;
		case FILE_STATUS_REMOVED:
			*status_str = _("Removed");
			break;
		case FILE_STATUS_NEEDS_CHECKOUT:
			*status_str = _("Needs Checkout");
			break;
		case FILE_STATUS_NEEDS_MERGE:
			*status_str = _("Needs Merge");
			break;
		case FILE_STATUS_MISSING:
			*status_str = _("Missing");
			break;
		case FILE_STATUS_CONFLICT:
			*status_str = _("Conflict");
			break;
		}
	}
}

static GdkPixbuf*
get_file_icon (ApothekeFileStatus status, gboolean is_directory)
{
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *pixbuf_icon = NULL;
	int width, height;
	double factor;

	if (is_directory)
		pixbuf = gdk_pixbuf_new_from_file (DATADIR "/pixmaps/apotheke/cvs-directory.png", NULL);
	else {
		switch (status) {
		case FILE_STATUS_UP_TO_DATE:
			pixbuf = gdk_pixbuf_new_from_file (DATADIR "/pixmaps/apotheke/cvs-file.png", NULL);
			break;
		case FILE_STATUS_MODIFIED:
			pixbuf = gdk_pixbuf_new_from_file (DATADIR "/pixmaps/apotheke/cvs-file-modified.png", NULL);
			break;
		case FILE_STATUS_MISSING:
			pixbuf = gdk_pixbuf_new_from_file (DATADIR "/pixmaps/apotheke/cvs-file-missing.png", NULL);
			break;
		default:
			pixbuf = NULL;
		}
	}
     
	if (pixbuf == NULL) return NULL;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	factor = (double) LIST_VIEW_ICON_HEIGHT / (double) height;
	pixbuf_icon = gdk_pixbuf_scale_simple (pixbuf, width * factor,
					       LIST_VIEW_ICON_HEIGHT,
					       GDK_INTERP_BILINEAR);

	g_object_unref (pixbuf);

	return pixbuf_icon;
}

void
apotheke_directory_create_file_list (ApothekeDirectory *dir, gboolean hide_ignored)
{
	GList *it;
	GList *ignore_patterns = 0;
	GList *cvs_entries = 0;

	g_return_if_fail (APOTHEKE_IS_DIRECTORY (dir));

	gtk_list_store_clear (GTK_LIST_STORE (dir));

	/* obtain data structures to determine file status */
	ignore_patterns = apotheke_directory_get_ignore_pattern_list (dir);
	cvs_entries = apotheke_cvs_entries_get_entries (dir->priv->uri);

	/* create list of files */
	create_file_list (dir, &cvs_entries, hide_ignored, ignore_patterns);

	/* clean up */
	for (it = ignore_patterns; it != NULL; it = it->next)
		g_pattern_spec_free ((GPatternSpec*)it->data);
	g_list_free (ignore_patterns);

	for (it = cvs_entries; it != NULL; it = it->next) 
		g_strfreev ((gchar**)it->data);
	g_list_free (cvs_entries);
}
    
gchar* 
apotheke_directory_get_uri (ApothekeDirectory *dir)
{
	g_return_val_if_fail (APOTHEKE_IS_DIRECTORY (dir), NULL);
	
	return dir->priv->uri;
}

