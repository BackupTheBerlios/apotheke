#include <gtk/gtkbutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkentry.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>

#include "apotheke-cvs-dialogs.h"

#define APOTHEKE_CONF_DIFF_OPERATION             "/apps/apotheke/diff/operation"
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

typedef struct {
	GConfClient *client;
	gchar *key;
	int   number;
	GtkWidget *child_widget;
} RadioCBData; 

static void 
on_radiobutton_toggled (GObject *button, gpointer *data)
{
	RadioCBData *radio_data = (RadioCBData*) data; 
	gboolean active;

	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

	if (active) {
		gconf_client_set_int (radio_data->client, radio_data->key, 
				      radio_data->number, NULL);
	}

	if (radio_data->child_widget != NULL) {
		gtk_widget_set_sensitive (GTK_WIDGET (radio_data->child_widget), active);
	}
}

static void
init_radio_button (GtkWidget *widget, GConfClient *client, gchar *key, int number, GtkWidget *child_widget)
{
	RadioCBData *data;
	gboolean is_active;

	data = g_new0 (RadioCBData, 1);
	data->client = client;
	data->key = key;
	data->number = number;
	data->child_widget = child_widget;

	is_active = (number == gconf_client_get_int (client, key, NULL));	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), is_active);
	if (child_widget != NULL) {
		gtk_widget_set_sensitive (GTK_WIDGET (child_widget), is_active);
	}

	g_signal_connect_data (G_OBJECT (widget), "toggled", 
			       G_CALLBACK (on_radiobutton_toggled), data,
 	 		       (GClosureNotify) g_free, G_CONNECT_AFTER);
}

gboolean 
apotheke_cvs_dialog_diff_show (GConfClient *client, ApothekeOptionsDiff *options)
{
	GtkWidget *dlg;
	GladeXML *xml;
	GtkWidget *widget;
	GtkWidget *child;
	int result;

	xml = glade_xml_new (GLADE_DIR"/apotheke.glade", "cvs-diff-dialog", "apotheke");
	/* FIXME: show error dialog here */
	if (xml == NULL) return FALSE;

	dlg = glade_xml_get_widget (xml, "cvs-diff-dialog");

	/* setup gui */
	widget = glade_xml_get_widget (xml, "check_recurse");
	init_checkbox_button (widget, client, APOTHEKE_CONF_DIFF_RECURSIVE);

	widget = glade_xml_get_widget (xml, "check_unified");
	init_checkbox_button (widget, client, APOTHEKE_CONF_DIFF_UNIFIED);

	widget = glade_xml_get_widget (xml, "check_addedremoved");
	init_checkbox_button (widget, client, APOTHEKE_CONF_DIFF_INCLUDE_ADDREMOVED);

	widget = glade_xml_get_widget (xml, "check_whitespaces");
	init_checkbox_button (widget, client, APOTHEKE_CONF_DIFF_IGNORE_WHITESPACES);

	widget = glade_xml_get_widget (xml, "radio_local");
	init_radio_button (widget, client, APOTHEKE_CONF_DIFF_OPERATION, 0, NULL);

	widget = glade_xml_get_widget (xml, "radio_local_tag");
	child = glade_xml_get_widget (xml, "local_tag_children");
	init_radio_button (widget, client, APOTHEKE_CONF_DIFF_OPERATION, 1, child);

	widget = glade_xml_get_widget (xml, "radio_tag_tag");
	child = glade_xml_get_widget (xml, "tag_tag_children");
	init_radio_button (widget, client, APOTHEKE_CONF_DIFF_OPERATION, 2, child);

	/* show dialog */
	result = gtk_dialog_run (GTK_DIALOG (dlg));

	/* init options struct */
	options->operation = gconf_client_get_int (client, APOTHEKE_CONF_DIFF_OPERATION, NULL);
	options->first_tag = NULL;
	options->first_is_date = FALSE;
	options->second_tag = NULL;
	options->second_is_date = FALSE;
	switch (options->operation) {
	case APOTHEKE_DIFF_COMPARE_LOCAL_TAG:
		widget = glade_xml_get_widget (xml, "entry_local_tag");
		options->first_tag = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
		widget = glade_xml_get_widget (xml, "check_local_tag_date");
		options->first_is_date = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
		break;
	case APOTHEKE_DIFF_COMPARE_TAG_TAG:
		widget = glade_xml_get_widget (xml, "entry_tag_tag1");
		options->first_tag = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
		widget = glade_xml_get_widget (xml, "check_tag_tag_date1");
		options->first_is_date = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));		
		widget = glade_xml_get_widget (xml, "entry_tag_tag2");
		options->second_tag = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
		widget = glade_xml_get_widget (xml, "check_tag_tag_date2");
		options->second_is_date = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));		
		break;
	case APOTHEKE_DIFF_COMPARE_LOCAL_REMOTE:
	default:
		break;
	}
	options->recursive = gconf_client_get_bool (client, APOTHEKE_CONF_DIFF_RECURSIVE, NULL);
	options->include_add_removed_files = gconf_client_get_bool (client, APOTHEKE_CONF_DIFF_UNIFIED, NULL);
	options->unified_diff = gconf_client_get_bool (client, APOTHEKE_CONF_DIFF_UNIFIED, NULL);
	options->whitespaces = gconf_client_get_bool (client, APOTHEKE_CONF_DIFF_IGNORE_WHITESPACES, NULL);

	gtk_widget_destroy (GTK_WIDGET (dlg));

	return (result == GTK_RESPONSE_OK);
}
