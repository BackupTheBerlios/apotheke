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
	GData     *file_list;
	gboolean  hide_ignored_files;
#ifdef HAVE_LIBFAM
	GnomeVFSMonitorHandle *monitor;
#endif
};

static void apotheke_directory_instance_init (ApothekeDirectory *dir);
static void apotheke_directory_class_init (ApothekeDirectoryClass *klass);
static void apotheke_directory_apply_cvs_status (ApothekeDirectory *dir);
static void apotheke_directory_apply_ignore_list (ApothekeDirectory *dir);
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
	priv->file_list = NULL;
	priv->hide_ignored_files = FALSE;
#ifdef HAVE_LIBFAM
	priv->monitor = NULL;
#endif
	g_datalist_init (&priv->file_list);
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
		GDK_TYPE_PIXBUF, /* COL_FILEICON, */
		G_TYPE_STRING,   /* COL_FILENAME, */
		G_TYPE_STRING,   /* COL_VERSION, */
		G_TYPE_STRING,   /* COL_STATUS, */
		G_TYPE_STRING,   /* COL_ATTRIBUTES, */
		G_TYPE_STRING,   /* COL_TAG, */
		G_TYPE_STRING,   /* COL_DATE, */
		G_TYPE_POINTER   /* COL_FILESTRUCT */
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

	if (ad->priv->file_list)
		g_datalist_clear (&ad->priv->file_list);
	ad->priv->file_list = NULL;

	g_free (ad->priv);
}


static gint 
apotheke_sort_func (GtkTreeModel *model,
		    GtkTreeIter *a,
		    GtkTreeIter *b,
		    gpointer user_data)
{
	ApothekeFile *af_a;
	ApothekeFile *af_b;
	int weight_a = 0;
	int weight_b = 0; 

	gtk_tree_model_get (model, a, AD_COL_FILESTRUCT, &af_a, -1);
	gtk_tree_model_get (model, b, AD_COL_FILESTRUCT, &af_b, -1);

	if (af_a->directory && !af_b->directory) {
		return -1;
	}
	else if (!af_a->directory && af_b->directory) {
		return 1;
	}
	else {
		if (af_a->type != FILE_TYPE_CVS &&
		    af_b->type == FILE_TYPE_CVS)
		{
			return -1;
		}
		if (af_b->status == FILE_TYPE_CVS &&
		    af_a->status >= FILE_TYPE_CVS)
		{
			return 1;
		}
		else {
			return g_utf8_collate (af_a->filename, af_b->filename);
		}
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
	ApothekeDirectory *ad;
	ApothekeFile *af;

	ad = APOTHEKE_DIRECTORY (user_data);

	switch (event_type) {
	case GNOME_VFS_MONITOR_EVENT_CHANGED:
		g_print ("Changed: %s  %s\n", monitor_uri, info_uri);
		af = get_apotheke_file_from_uri (ad, info_uri);
		if (af->status == FILE_STATUS_CVS_FILE) {
			af->status = FILE_STATUS_MODIFIED;
			/* FIXME: this function doesn't exist yet. */
			/* apply_apotheke_file_to_model (ad, af); */
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
}
#endif

static void
apotheke_directory_create_initial_file_list (ApothekeDirectory *dir)
{
	ApothekeDirectoryPrivate *priv;
	GnomeVFSResult result;
	GList *dir_list;
	GList *it;

	g_return_if_fail (APOTHEKE_IS_DIRECTORY (dir));
	
	priv = dir->priv;

	if (priv->uri == NULL) return;

	dir_list = NULL;
	result = gnome_vfs_directory_list_load (&dir_list, priv->uri, 
						GNOME_VFS_FILE_INFO_DEFAULT);
	if (result != GNOME_VFS_OK) {
		g_warning (_("Couldn't open directory %s: %s"),
			   priv->uri,
			   gnome_vfs_result_to_string (result));
		return;
	}
	
	for (it = dir_list; it != NULL; it = it->next) {
		ApothekeFile *af;
		GnomeVFSFileInfo *info;

		info = (GnomeVFSFileInfo*) it->data;
		
		if (!g_strcasecmp (info->name, "..") ||
		    !g_strcasecmp (info->name, ".") ||
		    !g_strcasecmp (info->name, "CVS")) 
		{
			gnome_vfs_file_info_unref (info);
			continue;
		}


		af = g_new0 (ApothekeFile, 1);
		
		af->filename = g_filename_to_utf8 (info->name, -1, NULL, NULL, NULL);
		af->quark = g_quark_from_string (info->name);
		af->directory = (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY);
		af->mtime = info->mtime;
		af->type = FILE_TYPE_NOT_IN_CVS;
		af->status = FILE_STATUS_NONE;
		
		gnome_vfs_file_info_unref (info);

		g_datalist_id_set_data (&(priv->file_list), 
					af->quark,
					af);
	}

	g_list_free (dir_list);

#ifdef HAVE_LIBFAM
	result = gnome_vfs_monitor_add (&priv->monitor,
					priv->uri,
					GNOME_VFS_MONITOR_DIRECTORY,
					monitor_callback,
					dir);
	if (result != GNOME_VFS_OK) 
		priv->monitor = NULL;
#endif
}

static void
apply_cvs_status_to_apotheke_file (char **entry, ApothekeFile *af)
{	
	time_t  cvs_time_t;
	struct tm tm_entry;

	/* To make the code clearer we have two steps here: First we check
	 * which status the files has. Then on base of the status we fill in
	 * the rest of the information.
	 */
	if (entry[CVS_ENTRIES_DIR][0] == 'D') {
		if (af->directory) {
			af->type = FILE_TYPE_CVS;
			af->status = FILE_STATUS_UP_TO_DATE;
		}
		else {
			af->type = FILE_TYPE_UNKNOWN;
			af->status = FILE_STATUS_NONE;
			g_warning (_("Oops, directory %s has no D flag in CVS/Entries."),
				   af->filename);
		}
	}
	else if (af->type == FILE_TYPE_NOT_IN_CVS) {
		/* these are all other cases */
		af->type = FILE_TYPE_CVS;
		
		/* NOTE: CVS stores UTC time, not localtime. */
		strptime (entry[CVS_ENTRIES_DATE], "%a %h %e %T %Y", &tm_entry);
		cvs_time_t = timegm (&tm_entry);
		
		if (af->mtime > cvs_time_t) {
			if (entry[CVS_ENTRIES_REVISION][0] == '0' /*NOT '\0'*/) {
				af->status = FILE_STATUS_ADDED;
			}
			else {
				af->status = FILE_STATUS_MODIFIED;
			}
		}
		else {
			af->status = FILE_STATUS_UP_TO_DATE;
		}		
	}
	
	/* Second step: fill the rest of the attributes. */
	af->cvs_revision = NULL;
	af->cvs_date = NULL;
	af->cvs_option = NULL;
	af->cvs_tag = NULL;
		
	/* set revision attribute */
	if (entry[CVS_ENTRIES_REVISION][0] != '\0' &&
	    af->type == FILE_TYPE_CVS && 
	    (af->status == FILE_STATUS_MODIFIED ||
	     af->status == FILE_STATUS_UP_TO_DATE))
	{
		af->cvs_revision = g_strdup (entry[CVS_ENTRIES_REVISION]);
	}
	else if (entry[CVS_ENTRIES_REVISION][0] != '\0' &&
		 af->type == FILE_TYPE_CVS &&
		 af->status == FILE_STATUS_REMOVED) 
	{
		af->cvs_revision = g_strdup (entry[CVS_ENTRIES_REVISION]+1);
	}
	
	/* set date attribute */
	if (entry[CVS_ENTRIES_DATE][0] != '\0' &&
	    af->type == FILE_TYPE_CVS && 
	    (af->status == FILE_STATUS_MODIFIED ||
	     af->status == FILE_STATUS_UP_TO_DATE))
	{
		af->cvs_date = g_strdup (entry[CVS_ENTRIES_DATE]);
	}
	
	/* set option attribute. */
	if (entry[CVS_ENTRIES_OPTION][0] != '\0') {
		af->cvs_option = g_strdup (entry[CVS_ENTRIES_OPTION]);
	}
	
	/* set tag */
	if (entry[CVS_ENTRIES_TAG][0] != '\0') {
		af->cvs_tag = g_strdup (g_strstrip (entry[CVS_ENTRIES_TAG]+1));
	}
}

static void
apply_cvs_status_callback (char **entry, gpointer data)
{
	ApothekeDirectoryPrivate *priv;
	ApothekeFile *af;

	priv = APOTHEKE_DIRECTORY (data)->priv;
		
	if (entry[CVS_ENTRIES_FILENAME] == NULL) return;
	
	af = (ApothekeFile*) g_datalist_get_data (&priv->file_list, 
						  entry[CVS_ENTRIES_FILENAME]);

	if (af == NULL) {
		af = g_new0 (ApothekeFile, 1);
		
		af->filename = g_filename_to_utf8 (entry[CVS_ENTRIES_FILENAME], 
						   -1, NULL, NULL, NULL);
		af->quark = g_quark_from_string (entry[CVS_ENTRIES_FILENAME]);
		af->type = FILE_TYPE_CVS;
		
		if (entry[CVS_ENTRIES_REVISION][0] == '-') {
			af->status = FILE_STATUS_REMOVED;
		}
		else {
			af->status = FILE_STATUS_MISSING;
		}
		
		g_datalist_id_set_data (&priv->file_list, af->quark, af);
	}

	apply_cvs_status_to_apotheke_file (entry, af);
}

void 
apotheke_directory_apply_cvs_status (ApothekeDirectory *dir)
{
	g_return_if_fail (APOTHEKE_IS_DIRECTORY (dir));

	apotheke_cvs_entries_for_each (dir->priv->uri, apply_cvs_status_callback, dir);
}


static void
check_for_ignore_status (GQuark quark, gpointer data, gpointer data_user)
{
	GList *pattern_list;
	GList *it;
	ApothekeFile *af;

	pattern_list = (GList*) data_user;
	af = (ApothekeFile*) data;

	for (it = pattern_list; (it != NULL && af->type != FILE_TYPE_IGNORE); it = it->next) 
	{
		if (g_pattern_match_string ((GPatternSpec*)it->data, af->filename)) {
			af->type = FILE_TYPE_IGNORE;
			af->status = FILE_STATUS_NONE;
		}
	}
}

void 
apotheke_directory_apply_ignore_list (ApothekeDirectory *dir)
{
	ApothekeDirectoryPrivate *priv;
	FILE *file;
	ApothekeFile *af;
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
	result = gnome_vfs_get_file_info (ignore_uri, info, GNOME_VFS_FILE_INFO_DEFAULT);
	if (result != GNOME_VFS_OK || info->type != GNOME_VFS_FILE_TYPE_REGULAR) {
		g_print (_("No .cvsignore file found.\n"));
		g_free (ignore_uri); 
		return;
	}
	gnome_vfs_file_info_unref (info);

	ignore_file = gnome_vfs_get_local_path_from_uri (ignore_uri);
	if (ignore_file == NULL) {
		g_free (ignore_uri);
		return;
	}

	file = fopen (ignore_file, "r");
	buffer = g_new0 (gchar, 256);
	
	while (fgets (buffer, 256, file)) {
		stripped = g_strdup (g_strstrip (buffer));
		utf8 = g_convert (stripped, -1, "Latin1", "UTF-8", NULL, NULL, NULL);
		pattern = g_pattern_spec_new (utf8);
		g_free (utf8);
		g_free (stripped);

		pattern_list = g_list_append (pattern_list, pattern);
	}

	fclose (file);
	g_free (buffer);
	g_free (ignore_file);
	g_free (ignore_uri);

	g_datalist_foreach (&priv->file_list, 
			    check_for_ignore_status, 
			    pattern_list);

	for (it = pattern_list; it != NULL; it = it->next)
		g_pattern_spec_free ((GPatternSpec*)it->data);
}

static void
get_status_text (ApothekeFile *af, gchar **status_str)
{
	*status_str = "";

	switch (af->type) {
	case FILE_TYPE_UNKNOWN:
		*status_str = _("Unknown");
		break;
	case FILE_TYPE_NOT_IN_CVS:
		*status_str = _("Not in CVS");
		break;
	case FILE_TYPE_IGNORE:
		*status_str = _("Ignore");
		break;
	case FILE_TYPE_CVS:
		if (af->directory)
			*status_str = _("Directory");
		else {
			switch (af->status) {
			case FILE_STATUS_NONE:
				*status_str = _("None");
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
}

static GdkPixbuf*
get_file_icon (ApothekeFile *file)
{
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *pixbuf_icon = NULL;
	int width, height;
	double factor;

	if (file == NULL) return NULL;
	if (file->type != FILE_TYPE_CVS) return NULL;

	if (file->directory)
		pixbuf = gdk_pixbuf_new_from_file (DATADIR "/pixmaps/apotheke/cvs-directory.png", NULL);
	else {
		switch (file->status) {
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

static void
add_file_to_tree (GQuark key, gpointer data, gpointer user_data)
{
	GtkTreeIter iter;
	GtkListStore *store;
	ApothekeDirectory *dir;
	ApothekeFile *af;
	gchar *status;
	gchar *flag;

	store = GTK_LIST_STORE (user_data);
	dir = APOTHEKE_DIRECTORY (user_data);
	af = (ApothekeFile*) data;

	if (dir->priv->hide_ignored_files &&
	    af->type == FILE_TYPE_IGNORE) 
	{
		return;
	}
	
	get_status_text (af, &status);
	
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    AD_COL_FILEICON, get_file_icon (af),
			    AD_COL_FILENAME, af->filename,
			    AD_COL_VERSION, af->cvs_revision ? af->cvs_revision : "",
			    AD_COL_STATUS, status,
			    AD_COL_ATTRIBUTES, af->cvs_option ? af->cvs_option : "",
			    AD_COL_TAG, af->cvs_tag ? af->cvs_tag : "",
			    AD_COL_DATE, af->cvs_date ? af->cvs_date : "",
			    AD_COL_FILESTRUCT, af,
			    -1);
}

void
apotheke_directory_create_file_list (ApothekeDirectory *dir)
{
	g_return_if_fail (APOTHEKE_IS_DIRECTORY (dir));

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (dir),
						 apotheke_sort_func,
						 NULL, NULL);

	apotheke_directory_create_initial_file_list (dir);
	apotheke_directory_apply_cvs_status (dir);
	apotheke_directory_apply_ignore_list (dir);

	/* populate the list model */
	g_datalist_foreach (&dir->priv->file_list,
			    add_file_to_tree,
			    dir);

}

static void
add_file_to_tree_if_ignored (GQuark key, gpointer data, gpointer user_data)
{
	ApothekeFile *af;

        af = (ApothekeFile*) data;
	if (af->type == FILE_TYPE_IGNORE)
		add_file_to_tree (key, data, user_data);
}

static void
show_ignored_files (ApothekeDirectory *dir) 
{
	g_assert (dir->priv->hide_ignored_files == FALSE);

	g_datalist_foreach (&dir->priv->file_list, 
			    add_file_to_tree_if_ignored,
			    dir);
}

static void
hide_ignored_files (ApothekeDirectory *dir) 
{
	GtkTreeIter iter;
	gboolean valid;
	ApothekeFile *af;
	
	g_assert (dir->priv->hide_ignored_files == TRUE);

	valid = gtk_tree_model_get_iter_root (GTK_TREE_MODEL (dir), &iter);
	if (!valid) return;

	do {
		gtk_tree_model_get (GTK_TREE_MODEL (dir),
				    &iter, 
				    AD_COL_FILESTRUCT,
				    &af, -1);

		if (af->type == FILE_TYPE_IGNORE) {
			gtk_list_store_remove (GTK_LIST_STORE (dir), &iter);
		}
		else {
			valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (dir),
							  &iter);
		}
		
	} while (valid && iter.stamp != 0);
}

void 
apotheke_directory_set_hide_ignored_files (ApothekeDirectory *dir, 
					   gboolean hide)
{
	g_return_if_fail (APOTHEKE_IS_DIRECTORY (dir));

	if (dir->priv->hide_ignored_files == hide) return;

	dir->priv->hide_ignored_files = hide;

	if (hide) {
		hide_ignored_files (dir);
	}
	else 
		show_ignored_files (dir);
}
    
gchar* 
apotheke_directory_get_uri (ApothekeDirectory *dir)
{
	g_return_val_if_fail (APOTHEKE_IS_DIRECTORY (dir), NULL);
	
	return dir->priv->uri;
}

static void
dump_file (GQuark key, gpointer data, gpointer user_data)
{
	ApothekeFile *af;
	gchar *flag = " ";
	gchar *status = "\t";

	af = (ApothekeFile*) data;

	if (!af->directory) {
		switch (af->type) {
		case FILE_TYPE_UNKNOWN:
			flag = "&";
			status = "Unknown";
			break;
		case FILE_TYPE_NOT_IN_CVS:
			flag = "?";
			status = "Not in CVS";
			break;
		case FILE_TYPE_IGNORE:
			flag = "I";
			status = "Ignore";
			break;
		case FILE_TYPE_CVS:
			switch (af->status) {
			case FILE_STATUS_UP_TO_DATE:
				status = "Up-To-Date";
				break;
			case FILE_STATUS_NEEDS_PATCH:
				status = "Needs Patch";
				break;
			case FILE_STATUS_ADDED:
				status = "Added";
				break;
			case FILE_STATUS_MODIFIED:
				status = "Modified";
				break;
			case FILE_STATUS_REMOVED:
				status = "Removed";
				break;
			case FILE_STATUS_NEEDS_CHECKOUT:
				status = "Needs Checkout";
				break;
			case FILE_STATUS_NEEDS_MERGE:
				status = "Needs Merge";
				break;
			}
		}
	}
		
	g_print ("%s | %s | %s | %s | %s | %s | %s\n", 
		 flag, 
		 af->filename, 
		 status,
		 af->cvs_revision ? af->cvs_revision : "\t",
		 af->cvs_date ? af->cvs_date : "\t",  
		 af->cvs_option ? af->cvs_option : "\t",
		 af->cvs_tag ? af->cvs_tag : "\t");
}


void
apotheke_directory_dump (ApothekeDirectory *ad)
{
	g_datalist_foreach (&ad->priv->file_list, dump_file, NULL);
}
