#ifndef __APOTHEKE_CVS_DIALOGS_H__
#define __APOTHEKE_CVS_DIALOGS_H__

#include <gconf/gconf-client.h>
#include "apotheke-options.h"

gboolean apotheke_cvs_dialog_diff_show (GConfClient *client, ApothekeOptionsDiff *options);


#endif /* __APOTHEKE_CVS_DIALOGS_H__ */
