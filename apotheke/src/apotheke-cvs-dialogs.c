#include <glib/gconvert.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkentry.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>

#include "apotheke-cvs-dialogs.h"

#define APOTHEKE_CONF_DIFF_OPERATION             "/apps/apotheke/diff/operation"
#define APOTHEKE_CONF_DIFF_RECURSIVE             "/apps/apotheke/diff/recursive"
#define APOTHEKE_CONF_DIFF_UNIFIED               "/apps/apotheke/diff/unified_diff"
#define APOTHEKE_CONF_DIFF_INCLUDE_ADDREMOVED    "/apps/apotheke/diff/include_addremoved"
#define APOTHEKE_CONF_DIFF_IGNORE_WHITESPACES    "/apps/apotheke/diff/ignore_whitespaces"
#define APOTHEKE_CONF_COMMIT_RECURSIVE           "/apps/apotheke/commit/recursive"
#define APOTHEKE_CONF_UPDATE_STICKY_OPTIONS      "/apps/apotheke/update/sticky_options"
#define APOTHEKE_CONF_UPDATE_RECURSIVE           "/apps/apotheke/update/recursive"
#define APOTHEKE_CONF_UPDATE_CREATE_DIRS         "/apps/apotheke/update/create_dirs"

typedef struct {
	GConfClient *client;
	gchar *key;
} ToggleCBData; 


static GladeXML*
get_xml_file (gchar *dialog_name)
{
	GladeXML *xml;

	xml = glade_xml_new (GLADE_DIR"/apotheke.glade", dialog_name, "apotheke");
	if (xml == NULL) {
		GtkWidget *dlg;

		dlg = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
					      GTK_MESSAGE_ERROR,
					      GTK_BUTTONS_CLOSE,
					      "Couldn't create dialog, file %s not found.",
					      GLADE_DIR"/apotheke.glade");
		gtk_signal_connect_object (GTK_OBJECT (dlg), "response",
					   GTK_SIGNAL_FUNC (gtk_widget_destroy),
					   GTK_OBJECT (dlg));
		gtk_widget_show (dlg);
	}

	return xml;
}


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

	xml = get_xml_file ("cvs-diff-dialog");
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
	g_object_unref (xml);

	return (result == GTK_RESPONSE_OK);
}


gboolean 
apotheke_cvs_dialog_commit_show (GConfClient *client, ApothekeOptionsCommit *options)
{
	GtkWidget *dlg;
	GladeXML *xml;
	GtkWidget *widget;
	gchar *message;
	int result;
	GtkTextIter start_iter;
	GtkTextIter end_iter;
	GtkTextBuffer *buffer;
	gchar *utf_txt;

	xml = get_xml_file ("cvs-commit-dialog");
	if (xml == NULL) return FALSE;

	dlg = glade_xml_get_widget (xml, "cvs-commit-dialog");
	gtk_window_resize (GTK_WINDOW (dlg), 300, 200);

	widget = glade_xml_get_widget (xml, "log_message");
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

	if (options->message != NULL) {
		utf_txt = g_locale_to_utf8 (options->message, -1, NULL, NULL, NULL);
		gtk_text_buffer_set_text (buffer, utf_txt, -1);

		g_free (options->message);
		options->message = NULL;
	}

	widget = glade_xml_get_widget (xml, "check_recurse");
	init_checkbox_button (widget, client, APOTHEKE_CONF_COMMIT_RECURSIVE);

	result = gtk_dialog_run (GTK_DIALOG (dlg));

	options->recursive = gconf_client_get_bool (client, APOTHEKE_CONF_COMMIT_RECURSIVE, NULL);
	/* obtain message entered by the user */
	gtk_text_buffer_get_start_iter (buffer, &start_iter);
	gtk_text_buffer_get_end_iter (buffer, &end_iter);
	utf_txt = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
	options->message = g_locale_from_utf8 (utf_txt, -1, NULL, NULL, NULL);

	gtk_widget_destroy (GTK_WIDGET (dlg));
	g_object_unref (xml);

	return (result == GTK_RESPONSE_OK);
}

gboolean 
apotheke_cvs_dialog_update_show (GConfClient *client, ApothekeOptionsUpdate *options)
{
	GtkWidget *dlg;
	GladeXML *xml;
	GtkWidget *widget;
	GtkWidget *child;
	int result;

	xml = get_xml_file ("cvs-update-dialog");

	if (xml == NULL) return FALSE;

	dlg = glade_xml_get_widget (xml, "cvs-update-dialog");

	/* setup dialog */
	widget = glade_xml_get_widget (xml, "check_recurse");
	init_checkbox_button (widget, client, APOTHEKE_CONF_UPDATE_RECURSIVE);

	widget = glade_xml_get_widget (xml, "check_create_dirs");
	init_checkbox_button (widget, client, APOTHEKE_CONF_UPDATE_CREATE_DIRS);

	widget = glade_xml_get_widget (xml, "radio_dont_change");
	init_radio_button (widget, client, APOTHEKE_CONF_UPDATE_STICKY_OPTIONS, 0, NULL);

	widget = glade_xml_get_widget (xml, "radio_reset");
	init_radio_button (widget, client, APOTHEKE_CONF_UPDATE_STICKY_OPTIONS, 1, NULL);

	widget = glade_xml_get_widget (xml, "radio_date");
	child = glade_xml_get_widget (xml, "entry_date");
	init_radio_button (widget, client, APOTHEKE_CONF_UPDATE_STICKY_OPTIONS, 2, child);


	widget = glade_xml_get_widget (xml, "radio_tag");
	child = glade_xml_get_widget (xml, "entry_tag");
	init_radio_button (widget, client, APOTHEKE_CONF_UPDATE_STICKY_OPTIONS, 3, child);

	result = gtk_dialog_run (GTK_DIALOG (dlg));

	/* update option struct */
	options->sticky_options = gconf_client_get_int (client, 
							APOTHEKE_CONF_UPDATE_STICKY_OPTIONS, 
							NULL);
	switch (options->sticky_options) {
	case APOTHEKE_UPDATE_STICKY_DATE:
		widget = glade_xml_get_widget (xml, "entry_date");
		options->sticky_tag = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
		break;
	case APOTHEKE_UPDATE_STICKY_TAG:
		widget = glade_xml_get_widget (xml, "entry_tag");
		options->sticky_tag = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
		break;
	default:
		options->sticky_tag = NULL;
	};
	options->recursive = gconf_client_get_bool (client, APOTHEKE_CONF_UPDATE_RECURSIVE, NULL);
	options->create_dirs = gconf_client_get_bool (client, 
						      APOTHEKE_CONF_UPDATE_CREATE_DIRS, 
						      NULL);

	gtk_widget_destroy (GTK_WIDGET (dlg));
	g_object_unref (xml);

	return (result == GTK_RESPONSE_OK);
}
