#include <glib.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include "apotheke-directory.h"

int
main (int argc, char **argv)
{
	ApothekeDirectory *ad;
	const gchar *dir = "/home/jens/gnome-cvs/eog";

	gnome_vfs_init ();
	
	ad = apotheke_directory_new (dir);

	apotheke_directory_create_file_list (ad);

	apotheke_directory_apply_cvs_status (ad);
	apotheke_directory_apply_ignore_list (ad);

	apotheke_directory_dump (ad);

	return 0;
}
