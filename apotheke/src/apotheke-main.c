#include <config.h>

#include <libnautilus/nautilus-view-standard-main.h>

#include "apotheke-view.h"

#define FACTORY_IID     "OAFIID:apotheke_view_factory:2002-03-09-23-02-15"
#define VIEW_IID        "OAFIID:apotheke_view:2002-03-09-23-02-15"

static int global_argc;
static char **global_argv;

static void
post_init_callback (void)
{
	if (!gconf_is_initialized ())
		gconf_init (global_argc, global_argv, NULL);	
}

int
main (int argc, char *argv[])
{
	global_argc = argc;
	global_argv = argv;

	return nautilus_view_standard_main ("apotheke-view",
					    VERSION,
					    GETTEXT_PACKAGE,
					    GNOMELOCALEDIR,
					    argc,
					    argv,
					    FACTORY_IID,
					    VIEW_IID,
					    nautilus_view_create_from_get_type_function,
					    post_init_callback,
					    apotheke_view_get_type);
}
