#include <config.h>

#include "apotheke-view.h"
#include <libnautilus/nautilus-view-standard-main.h>

#define FACTORY_IID     "OAFIID:apotheke_view_factory:2002-03-09-23-02-15"
#define VIEW_IID        "OAFIID:apotheke_view:2002-03-09-23-02-15"

int
main (int argc, char *argv[])
{
	return nautilus_view_standard_main ("apotheke-view",
					    VERSION,
					    GETTEXT_PACKAGE,
					    GNOMELOCALEDIR,
					    argc,
					    argv,
					    FACTORY_IID,
					    VIEW_IID,
					    nautilus_view_create_from_get_type_function,
					    NULL,
					    apotheke_view_get_type);
}
