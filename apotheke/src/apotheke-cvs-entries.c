#include <stdio.h>

#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnome/gnome-i18n.h>

#include "apotheke-cvs-entries.h"

static GnomeVFSResult
get_cvs_entries_path (char *uri, char** path)
{
	char *entries_uri;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result; 
	
	*path = NULL;

	entries_uri = g_build_filename (uri, "CVS", "Entries", NULL);

	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (entries_uri, info, GNOME_VFS_FILE_INFO_DEFAULT);

	if (result != GNOME_VFS_OK || info->type != GNOME_VFS_FILE_TYPE_REGULAR) {
		if (result == GNOME_VFS_OK)
			result = GNOME_VFS_ERROR_IS_DIRECTORY;
	}
	else {
		*path = gnome_vfs_get_local_path_from_uri (entries_uri);
		result = (*path == NULL) ? GNOME_VFS_ERROR_GENERIC : GNOME_VFS_OK;
	}
	
	g_free (entries_uri);
	gnome_vfs_file_info_unref (info);

	return result;
}


char** 
apotheke_cvs_entries_get_entry (char* uri, char* filename)
{
	FILE *file;
	gchar *entries_path;
	gchar *buffer;
	gchar **strings;
	GnomeVFSResult result;
	
	result = get_cvs_entries_path (uri, &entries_path);
	if (result != GNOME_VFS_OK) {
		g_print (_("Couldn't open CVS Entries file for: %s\n"), uri);
		return NULL;
	}

	file = fopen (entries_path, "r");
	buffer = g_new0 (gchar, 512);

	while (fgets (buffer, 512, file)) {
		
		strings = g_strsplit (buffer, "/", 6);

		if (g_ascii_strcasecmp (strings[CVS_ENTRIES_FILENAME], filename) == 0)
			break;

		g_strfreev (strings);
		strings = NULL;
	}

	g_free (buffer);
	fclose (file);

	return strings;
}

void  
apotheke_cvs_entries_for_each (char *uri, ApothekeCVSEntriesCallback callback, gpointer data)
{
	FILE *file;
	gchar *entries_path;
	gchar *buffer;
	gchar **strings;
	GnomeVFSResult result;

	result = get_cvs_entries_path (uri, &entries_path);
	if (result != GNOME_VFS_OK) {
		g_print (_("Couldn't open CVS Entries file for: %s\n"), uri);
		return;
	}

	file = fopen (entries_path, "r");
	buffer = g_new0 (gchar, 512);
	
	while (fgets (buffer, 512, file)) {
		
		strings = g_strsplit (buffer, "/", 6);

		(*callback) (strings, data);

		g_strfreev (strings);
	}

	g_free (buffer);
	fclose (file);
}
