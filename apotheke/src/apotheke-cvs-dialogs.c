#include <gtk/gtkbutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkcheckbutton.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>

#include "apotheke-cvs-dialogs.h"

#define APOTHEKE_CONF_DIFF_RECURSIVE             "/apps/apotheke/diff/recursive"
#define APOTHEKE_CONF_DIFF_UNIFIED               "/apps/apotheke/diff/unified_diff"
#define APOTHEKE_CONF_DIFF_INCLUDE_ADDREMOVED    "/apps/apotheke/diff/include_addremoved"
#define APOTHEKE_CONF_DIFF_IGNORE_WHITESPACES    "/apps/apotheke/diff/ignore_whitespaces"

typedef struct {
	GConfClient *client;
	gchar *key;
} ToggleCBData; 


static void 
on_checkbutton_toggled (GObject *button, gpointer *data)
{
	ToggleCBData *toggle_data = (ToggleCBData*) data; 
	gboolean active;

	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

	gconf_client_set_bool (toggle_data->client, toggle_data->key, 
			      active, NULL);
}

static void
init_checkbox_button (GtkWidget *widget, GConfClient *client, gchar *key)
{
	ToggleCBData *data;

	data = g_new0 (ToggleCBData, 1);
	data->client = client;
	data->key = key;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				      gconf_client_get_bool (client, key, NULL));

	g_signal_connect_data (G_OBJECT (widget), "toggled", 
			       G_CALLBACK (on_checkbutton_toggled), data,
 	 		       (GClosureNotify) g_free, G_CONNECT_AFTER);
}

gboolean 
apotheke_cvs_dialog_diff_show (GConfClient *client, ApothekeOptionsDiff *options)
{
	GtkWidget *dlg;
	GladeXML *xml;
	GtkWidget *widget;
	int result;

	xml = glade_xml_new (GLADE_DIR"/apotheke.glade", "cvs-diff-dialog", "apotheke");
	/* FIXME: show error dialog here */
	if (xml == NULL) return FALSE;

	dlg = glade_xml_get_widget (xml, "cvs-diff-dialog");

	/* connect signals */
	widget = glade_xml_get_widget (xml, "check_recurse");
	init_checkbox_button (widget, client, APOTHEKE_CONF_DIFF_RECURSIVE);

	widget = glade_xml_get_widget (xml, "check_unified");
	init_checkbox_button (widget, client, APOTHEKE_CONF_DIFF_UNIFIED);

	widget = glade_xml_get_widget (xml, "check_addedremoved");
	init_checkbox_button (widget, client, APOTHEKE_CONF_DIFF_INCLUDE_ADDREMOVED);

	widget = glade_xml_get_widget (xml, "check_whitespaces");
	init_checkbox_button (widget, client, APOTHEKE_CONF_DIFF_IGNORE_WHITESPACES);

	result = gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (GTK_WIDGET (dlg));

	/* init options struct */
	options->recursive = gconf_client_get_bool (client, APOTHEKE_CONF_DIFF_RECURSIVE, NULL);
	options->include_add_removed_files = gconf_client_get_bool (client, APOTHEKE_CONF_DIFF_UNIFIED, NULL);
	options->unified_diff = gconf_client_get_bool (client, APOTHEKE_CONF_DIFF_UNIFIED, NULL);
	options->whitespaces = gconf_client_get_bool (client, APOTHEKE_CONF_DIFF_IGNORE_WHITESPACES, NULL);

	return (result == GTK_RESPONSE_OK);
}
