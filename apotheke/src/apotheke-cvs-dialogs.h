#ifndef __APOTHEKE_CVS_DIALOGS_H__
#define __APOTHEKE_CVS_DIALOGS_H__

#include <gconf/gconf-client.h>
#include "apotheke-options.h"

gboolean apotheke_cvs_dialog_diff_show (GConfClient *client, ApothekeOptionsDiff *options);

gboolean apotheke_cvs_dialog_commit_show (GConfClient *client, ApothekeOptionsCommit *options);

gboolean apotheke_cvs_dialog_update_show (GConfClient *client, ApothekeOptionsUpdate *options);

#endif /* __APOTHEKE_CVS_DIALOGS_H__ */
