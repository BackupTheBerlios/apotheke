#include <config.h>
#include <glib/gtypes.h>
#include <gdk/gdkevents.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>

#include "apotheke-view.h"
#include "apotheke-directory.h"
#include "apotheke-client-cvs.h"
#include "apotheke-highlight-buffer.h"

#define GUTTER_HIDDEN_THRESHOLD 4

typedef struct {
	GtkWidget *paned;
	int saved_position;
	double press_y;
	guint32 press_time;
} GutterInfo;

struct _ApothekeViewPrivate {
	ApothekeDirectory *ad;

	GtkWidget    *event_box;
	
	GtkWidget     *tree_view;
	GtkWidget     *text_view;
	GtkTextBuffer *console;
	BonoboUIComponent *ui_component;

	ApothekeClientCVS *client;
	GConfClient   *config;

	gboolean     hide_ignored_files;

	GutterInfo   gutter;

	GData        *icon_cache;
};

/*
 * The row height should be large enough to not clip emblems.
 * Computing this would be costly, so we just choose a number
 * that works well with the set of emblems we've designed.
 */
#define LIST_VIEW_MINIMUM_ROW_HEIGHT	28
#define LIST_VIEW_ICON_HEIGHT           24

#define ID_HIDE_IGNORED_FILES           "Hide Ignored Files"

#define APOTHEKE_CONFIG_DIR              "/apps/apotheke"
#define APOTHEKE_CONFIG_CONSOLE_POS      "/apps/apotheke/view/console_pos"
#define APOTHEKE_CONFIG_HIDE_IGNORED     "/apps/apotheke/view/hide_ignored"

static void apotheke_view_class_init (ApothekeViewClass *klass);
static void apotheke_view_instance_init (ApothekeView *view);
static void apotheke_view_finalize (GObject *object);
static void apotheke_view_destroy (BonoboObject *object);

static void apotheke_view_load_location_callback (NautilusView *nautilus_view, 
						  const char *location,
						  ApothekeView *view);
static void apotheke_view_load_uri (ApothekeView *view, const char *location);
static gboolean console_is_hidden (ApothekeView *view);
static void show_console (ApothekeView *view);
static void hide_console (ApothekeView *view);

BONOBO_CLASS_BOILERPLATE (ApothekeView, apotheke_view, 
			  NautilusView, NAUTILUS_TYPE_VIEW)

static void
apotheke_view_class_init (ApothekeViewClass *klass)
{
	GObjectClass *gobject_class;
	BonoboObjectClass *object_class;

	gobject_class = G_OBJECT_CLASS (klass);
	object_class = BONOBO_OBJECT_CLASS (klass);

	object_class->destroy = apotheke_view_destroy;
        gobject_class->finalize = apotheke_view_finalize;
}

#define CREATE_COLUMN(id, align, min_width)       \
        cell = gtk_cell_renderer_text_new (); \
	g_object_set (G_OBJECT (cell),        \
		      "xalign", align,        \
		      NULL);                  \
 	gtk_cell_renderer_set_fixed_size (cell, -1, LIST_VIEW_MINIMUM_ROW_HEIGHT); \
        column = gtk_tree_view_column_new_with_attributes (titles[ id ], \
		                                           cell,    \
                                                           "text", id, \
							   NULL); \
        gtk_tree_view_column_set_alignment (column, align); \
        if (min_width > -1)                                       \
                gtk_tree_view_column_set_min_width (column, min_width); \
        gtk_tree_view_column_set_resizable (column, TRUE); \
        gtk_tree_view_column_set_sort_column_id (column, id); \
        gtk_tree_view_append_column (tree_view, column)
        

static const gchar* titles[] = {
	"",
	N_("Name"),
	N_("Revision"),
	N_("Status"),
	N_("Option"),
	N_("Tag"),
	N_("Date")
};


static void
set_up_tree_view (GtkTreeView *tree_view)
{
	GtkCellRenderer *cell;
        GtkTreeViewColumn *column;

	/* filename column */
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_renderer_set_fixed_size (cell, -1, LIST_VIEW_MINIMUM_ROW_HEIGHT);
	
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_sort_column_id (column, AD_COL_FILENAME);
	gtk_tree_view_column_set_title (column, titles[AD_COL_FILENAME]);
	
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_attributes (column, cell,
					     "pixbuf", AD_COL_FILEICON,
					     NULL);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_attributes (column, cell,
					     "text", AD_COL_FILENAME,
					     NULL);
        gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (tree_view, column);

	CREATE_COLUMN (AD_COL_VERSION, 0.01, 100);
	CREATE_COLUMN (AD_COL_STATUS, 0.01, 100);
	CREATE_COLUMN (AD_COL_ATTRIBUTES, 0.5, 100);
	CREATE_COLUMN (AD_COL_TAG, 0.01, 100);
	CREATE_COLUMN (AD_COL_DATE, 0.01, 100);
}


static void
hide_ignored_files_state_changed_callback (BonoboUIComponent   *component,
					   const char          *path,
					   Bonobo_UIComponent_EventType type,
					   const char          *state,
					   gpointer            user_data)
{
	ApothekeView *view;
	gboolean hide_files;

	view = (ApothekeView*) user_data;

	if (g_strcasecmp (state, "") == 0) {
		/* State goes blank when component is removed; ignore this. */
		return;
	}

	hide_files = (g_ascii_strcasecmp (state, "1") == 0);

	gconf_client_set_int (view->priv->config, APOTHEKE_CONFIG_HIDE_IGNORED,
			      (int) hide_files, NULL);
}

static void
add_selected_file_to_list (GtkTreeModel *model, GtkTreePath *path, 
			   GtkTreeIter *iter, gpointer data)
{
	GList **list;
	ApothekeFile *af;

	list = (GList**) data;

	gtk_tree_model_get (model, iter, AD_COL_FILESTRUCT, &af, -1);
	
        *list = g_list_append (*list, af->filename);
}

static void
execute_cvs_command (ApothekeView *view, ApothekeCommandType command, ApothekeOptions *options)
{
	GtkTreeSelection *selection;
	GList *files;
	GtkTextMark *mark;
	GtkTextIter iter;

	if (console_is_hidden (view))
		show_console (view);

	/* collect files to process */
	files = NULL;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->priv->tree_view));
	gtk_tree_selection_selected_foreach (selection,
					     add_selected_file_to_list,
					     &files);

	/* mark start of comand output */
	gtk_text_buffer_get_end_iter (view->priv->console, &iter);
	mark = gtk_text_buffer_create_mark (view->priv->console, NULL,
					    &iter, TRUE);

	/* execute command */
	if (apotheke_client_cvs_do (view->priv->client,
				    view->priv->ad,
				    command,
				    options, 
				    files)) 
	{
		/* highlight buffer if desired */
		gtk_text_buffer_get_iter_at_mark (view->priv->console, 
						  &iter, mark);
		apotheke_highlight_buffer_diff (view->priv->console, &iter);
		
	}
	else {
		g_warning ("Something did go wrong.\n");
	}

	g_list_free (files);
}

static void
verb_CVS_status_cb (BonoboUIComponent *uic, 
		    gpointer user_data,
		    const char *cname)
{
	ApothekeView *view;
	ApothekeOptionsStatus *options;

	g_print ("CVS STATUS. do it\n");

	view = APOTHEKE_VIEW (user_data);

	options = g_new0 (ApothekeOptionsStatus, 1);
	options->type = APOTHEKE_CMD_STATUS;
	options->compression = 0;

	options->recursive = FALSE;
	options->verbose = TRUE;
	
	execute_cvs_command (view, APOTHEKE_CMD_STATUS, (ApothekeOptions*) options);
}

static void
verb_Apotheke_revert_state_cb (BonoboUIComponent *uic, 
			       gpointer user_data,
			       const char *cname)
{
	/* FIXME: TODO */
}

static void
verb_CVS_diff_cb (BonoboUIComponent *uic, 
		  gpointer user_data,
		  const char *cname)
{
	ApothekeView *view;
	ApothekeOptionsDiff *options;

	g_print ("CVS DIFF. do it\n");

	view = APOTHEKE_VIEW (user_data);

	options = g_new0 (ApothekeOptionsDiff, 1);
	options->type = APOTHEKE_CMD_DIFF;
	options->compression = 0;

	options->recursive = FALSE;
	options->include_add_removed_files = FALSE;
	options->unified_diff = TRUE;

	execute_cvs_command (view, APOTHEKE_CMD_DIFF, (ApothekeOptions*) options);
}

static BonoboUIVerb apotheke_verbs[] = {
	BONOBO_UI_VERB ("CVS Status", verb_CVS_status_cb),
	BONOBO_UI_VERB ("CVS Diff", verb_CVS_diff_cb),
	BONOBO_UI_VERB_END
};


static void
merge_bonobo_ui_callback (BonoboControl *control, 
			  gboolean       state, 
			  gpointer       user_data)
{
 	ApothekeView *view;
	BonoboUIComponent *ui_component;
	gboolean hide_ignored;

	g_assert (BONOBO_IS_CONTROL (control));
	
	view = APOTHEKE_VIEW (user_data);

	if (state) {
		ui_component = nautilus_view_set_up_ui (NAUTILUS_VIEW (view),
							DATADIR,
							"apotheke-view-ui.xml",
							"apotheke-view");

		hide_ignored = (gconf_client_get_int (view->priv->config, APOTHEKE_CONFIG_HIDE_IGNORED, NULL) == 1);
		bonobo_ui_component_set_prop (ui_component,
					      "/commands/Hide Ignored Files",
					      "state",
					      hide_ignored ? "1" : "0",
					      NULL);
		
		bonobo_ui_component_add_listener (ui_component, 
						  ID_HIDE_IGNORED_FILES, 
						  hide_ignored_files_state_changed_callback, 
						  view);

		bonobo_ui_component_add_verb_list_with_data (ui_component, 
							     apotheke_verbs,
							     view);
		view->priv->ui_component = ui_component;
	}
}

static void
list_activate_callback (GtkTreeView *tree_view, GtkTreePath *path, 
			GtkTreeViewColumn *column, gpointer user_data)
{
	ApothekeView *view;
	GtkTreeIter iter;
	ApothekeFile *af;
	gchar *uri;
	gchar *filename;

	view = APOTHEKE_VIEW (user_data);
	
	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (view->priv->ad),
				     &iter, path))
	{
		gtk_tree_model_get (GTK_TREE_MODEL (view->priv->ad),
				    &iter, 
				    AD_COL_FILESTRUCT, &af,
				    -1);
		g_assert (af != NULL);

		filename = g_filename_from_utf8 (af->filename, -1, NULL, NULL, NULL);
		uri = g_build_filename (apotheke_directory_get_uri (view->priv->ad), 
					filename, NULL);

		nautilus_view_open_location_in_this_window (NAUTILUS_VIEW (view), 
							    uri);

		g_free (uri);
		g_free (filename);
	}
}

static void
handle_context_cvs_status (GtkWidget *widget, gpointer data)
{
	verb_CVS_status_cb (NULL, data, NULL);
}

static void
handle_context_cvs_diff (GtkWidget *widget, gpointer data)
{
	verb_CVS_diff_cb (NULL, data, NULL);
}

static void
handle_context_revert_state (GtkWidget *widget, gpointer data)
{
	verb_Apotheke_revert_state_cb (NULL, data, NULL);
}

static void
add_popup_item (ApothekeView *view, GtkWidget **menu, gchar *txt, gpointer callback)
{
	GtkWidget *label;
	GtkWidget *item;

	if (g_ascii_strcasecmp ("-", txt)) {
		label = gtk_label_new (txt);
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_widget_show (label);

		item = gtk_menu_item_new ();
		gtk_container_add (GTK_CONTAINER (item), label);
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (callback), view);
	}
	else {		
		item = gtk_separator_menu_item_new ();		
	}
       
	gtk_widget_show (item);

	gtk_menu_shell_append (GTK_MENU_SHELL (*menu), item);
}

/* This function is taken from eel/eel/eel-gtk-extensions.c
 * written by John Sullivan, Ramiro Estrugo and Darin Adler.
 */
static void
popup_menu_position_func (GtkMenu  *menu,
			  int      *x,
			  int      *y,
			  gboolean *push_in,
			  gpointer  user_data)
{
	GdkPoint *offset;
	GtkRequisition requisition;

	g_assert (x != NULL);
	g_assert (y != NULL);

	offset = (GdkPoint *) user_data;

	g_assert (offset != NULL);

	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);
	  
	*x = CLAMP (*x + (int) offset->x, 0, MAX (0, gdk_screen_width () - requisition.width));
	*y = CLAMP (*y + (int) offset->y, 0, MAX (0, gdk_screen_height () - requisition.height));

	*push_in = FALSE;
}


static void
handle_right_click (ApothekeView *view, GdkEventButton *event)
{
	GdkPoint offset;
	GtkWidget *menu;

	/* create context menu */
	menu = gtk_menu_new ();
	g_signal_connect (G_OBJECT (menu), "hide",
			  G_CALLBACK (g_object_unref), NULL);

	g_object_ref (G_OBJECT (menu));
	gtk_object_sink (GTK_OBJECT (menu));
	
	add_popup_item (view, &menu, _("CVS Status"), handle_context_cvs_status);
	add_popup_item (view, &menu, _("CVS Diff"), handle_context_cvs_diff);
	add_popup_item (view, &menu, "-", NULL);
	add_popup_item (view, &menu, _("Revert To Repository State"), handle_context_revert_state);
	
	/* display menu */
	gtk_widget_show_all (menu);

	offset.x = 2;
	offset.y = 2;
	
	gtk_menu_popup (GTK_MENU (menu), 
			NULL, 
			NULL, 
			popup_menu_position_func,
			&offset, 
			event->button,
			event ? event->time : GDK_CURRENT_TIME);
}

static void
event_after_callback (GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	ApothekeView *view;

	view = APOTHEKE_VIEW (data);

	if (event->type == GDK_BUTTON_PRESS
	    && event->window == gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget))
	    && (((GdkEventButton *) event)->button == 3)) {
		handle_right_click (view, (GdkEventButton*) event);
	}
}

/* This code is taken from nautilus/src/fm-list-view.c written by
 * John Sullivan, Anders Carlsson and David Emory Watson.
 */
static gint
handle_button_press_event (GtkWidget *widget, GdkEventButton *event,  gpointer data) 
{
	GtkTreeView *tree_view;
	GtkTreePath *path;

	tree_view = GTK_TREE_VIEW (widget);

	if (event->window != gtk_tree_view_get_bin_window (tree_view)) {
		return FALSE;
	}

	if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
					   &path, NULL, NULL, NULL)) {
		if (event->button == 3
		    && gtk_tree_selection_path_is_selected (gtk_tree_view_get_selection (tree_view), path)) {
			/* Don't let the default code run because if multiple rows
			   are selected it will unselect all but one row; but we
			   want the right click menu to apply to everything that's
			   currently selected. */
			return TRUE;
		}

		gtk_tree_path_free (path);
	} else {
		/* Deselect if people click outside any row. It's OK to
		   let default code run; it won't reselect anything. */
		gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (tree_view));
	}

	return FALSE;
}

static gboolean
console_is_hidden (ApothekeView *view)
{
	GtkWidget *paned;
	int height;

	paned = view->priv->gutter.paned;
	height = paned->allocation.height;

	return ((height - gtk_paned_get_position (GTK_PANED (paned))) < 10);
}

static void
show_console (ApothekeView *view)
{
	GutterInfo *info;

	info = &view->priv->gutter;
	gtk_paned_set_position (GTK_PANED (info->paned), info->saved_position);
}

static void
hide_console (ApothekeView *view)
{
	GutterInfo *info;
	int height;

	info = &view->priv->gutter;
	height = info->paned->allocation.height;

	info->saved_position = gtk_paned_get_position (GTK_PANED (info->paned));
	gtk_paned_set_position (GTK_PANED (info->paned), height);
}

static gint
gutter_button_press_callback (GtkWidget *widget, GdkEventButton *event,  gpointer data) 
{
	ApothekeView *view;
	
	view = APOTHEKE_VIEW (data);

	view->priv->gutter.press_y = event->y;
	view->priv->gutter.press_time = event->time;
	
	return FALSE;
}

static gint
gutter_button_release_callback (GtkWidget *widget, GdkEventButton *event,  gpointer data) 
{
	ApothekeView *view;
	int diff_y;
	int diff_time;
	
	view = APOTHEKE_VIEW (data);
	
	diff_y = abs (event->y - view->priv->gutter.press_y);
	diff_time = event->time - view->priv->gutter.press_time;
	
	if (diff_y < 1 && diff_time < 200 ) {
		if (console_is_hidden (view)) {
			show_console (view);
		}
		else {
			hide_console (view);
		}
	}
	
	gconf_client_set_int (view->priv->config, APOTHEKE_CONFIG_CONSOLE_POS,
			      gtk_paned_get_position (GTK_PANED (view->priv->gutter.paned)), NULL);

	return FALSE;
}

static GtkTextTagTable*
setup_tag_table (void)
{
	GtkTextTagTable *table;
	GtkTextTag *tag;

	table = gtk_text_tag_table_new ();
	
	/* diff tags */
	tag = gtk_text_tag_new ("diff-added");
	g_object_set (G_OBJECT (tag), 
		      "foreground", "black",
		      "background", "yellow",
		      NULL);
	gtk_text_tag_table_add (table, tag);

	tag = gtk_text_tag_new ("diff-removed");
	g_object_set (G_OBJECT (tag),
		      "foreground", "black",
		      "background", "RoyalBlue",
		      NULL);
	gtk_text_tag_table_add (table, tag);
	
	return table;
}

static void
construct_ui (ApothekeView *view)
{
	ApothekeViewPrivate *priv;
	GtkWidget *scroll_window;
	GtkWidget *vpane;
	GtkTextTagTable *tag_table;
	gchar *text;

	priv = view->priv;

	/* set up tree view */
        priv->tree_view = gtk_tree_view_new ();
	set_up_tree_view (GTK_TREE_VIEW (priv->tree_view));
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view)), 
				     GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->tree_view), TRUE);
	g_signal_connect (G_OBJECT (priv->tree_view), "row_activated", 
			  G_CALLBACK (list_activate_callback), view);
	g_signal_connect_object (priv->tree_view, "event-after",
				G_CALLBACK (event_after_callback), view, 0);
	g_signal_connect_object (priv->tree_view, "button_press_event",
				 G_CALLBACK (handle_button_press_event), view, 0);

	
	/* put it into a scroll window */
	scroll_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_widget_show (scroll_window);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll_window),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll_window),
                                             GTK_SHADOW_ETCHED_IN);
	gtk_container_add (GTK_CONTAINER (scroll_window), priv->tree_view);	

	/* vpane */
	vpane = gtk_vpaned_new ();
	priv->gutter.paned = vpane;
	g_signal_connect (G_OBJECT (vpane), "button-press-event",
			  G_CALLBACK (gutter_button_press_callback), view);
	g_signal_connect (G_OBJECT (vpane), "button-release-event",
			  G_CALLBACK (gutter_button_release_callback), view);
	gtk_paned_pack1 (GTK_PANED (vpane), scroll_window, TRUE, TRUE);
	
	/* text console */
	tag_table = setup_tag_table ();
	priv->console = gtk_text_buffer_new (tag_table);
	text = "Apotheke version " VERSION ", (C) 2002 Jens Finke <jens@triq.net>\n";
	gtk_text_buffer_set_text (priv->console, text, -1);
	priv->text_view = gtk_text_view_new_with_buffer (priv->console);
	scroll_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll_window), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll_window),
					     GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scroll_window), priv->text_view);
	gtk_paned_pack2 (GTK_PANED (vpane), scroll_window, TRUE, TRUE);

	gtk_container_add (GTK_CONTAINER (priv->event_box), vpane);
	gtk_widget_show_all (vpane);

	gtk_paned_set_position (GTK_PANED (priv->gutter.paned), 
				gconf_client_get_int (priv->config, APOTHEKE_CONFIG_CONSOLE_POS, NULL));
	priv->gutter.saved_position = gconf_client_get_int (priv->config, 
							    APOTHEKE_CONFIG_CONSOLE_POS, NULL);
}

static void
config_value_changed_cb (GConfClient *client,
			 guint cnxn_id,
			 GConfEntry *entry,
			 gpointer user_data)
{
	ApothekeView *view;

	view = APOTHEKE_VIEW (user_data);

	if (entry == NULL) return;

	if (g_ascii_strcasecmp (entry->key, APOTHEKE_CONFIG_HIDE_IGNORED) == 0) {
		gboolean hide;

		hide = (gconf_value_get_int (entry->value) == 1);
		if (view->priv->ad != NULL)
			apotheke_directory_set_hide_ignored_files (view->priv->ad, hide);
	}
}

static void
apotheke_view_instance_init (ApothekeView *view)
{
	ApothekeViewPrivate *priv;

	priv = g_new0 (ApothekeViewPrivate, 1);
	view->priv = priv;
	priv->hide_ignored_files = FALSE;
	priv->ad = NULL;
	priv->icon_cache = NULL;
	priv->ui_component = NULL;
	g_datalist_init (&priv->icon_cache);

	/* init gconf */
	g_assert (gconf_is_initialized ());
	priv->config = gconf_client_get_default ();
	gconf_client_add_dir (priv->config,
			      APOTHEKE_CONFIG_DIR,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);
	gconf_client_notify_add (priv->config,
				 APOTHEKE_CONFIG_DIR,
				 config_value_changed_cb,
				 view,
				 NULL, NULL);
			      
        priv->event_box = gtk_event_box_new ();
        gtk_widget_show (priv->event_box);

	nautilus_view_construct (NAUTILUS_VIEW (view), priv->event_box);

	g_signal_connect (view, 
			  "load_location",
			  G_CALLBACK (apotheke_view_load_location_callback), 
			  view);

	/* Get notified when our bonobo control is activated so we can
	 * merge menu & toolbar items into the shell's UI.
	 */
        g_signal_connect (nautilus_view_get_bonobo_control (NAUTILUS_VIEW (view)),
			  "activate",
			  G_CALLBACK (merge_bonobo_ui_callback),
			  view);

	construct_ui (view);

	priv->client = apotheke_client_cvs_new (priv->console);
}

static void
apotheke_view_destroy (BonoboObject *object)
{
	ApothekeViewPrivate *priv;

	priv = ((ApothekeView*) object)->priv;

	gconf_client_remove_dir (priv->config, APOTHEKE_CONFIG_DIR, NULL);
	g_object_unref (G_OBJECT (priv->config));

	g_object_unref (G_OBJECT (priv->ad));

	g_object_unref (G_OBJECT (priv->client));

	g_datalist_clear (&priv->icon_cache);
	priv->icon_cache = NULL;

	if (BONOBO_OBJECT_CLASS (parent_class)->destroy)
		BONOBO_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
apotheke_view_finalize (GObject *object)
{
	ApothekeView *view;

	view = APOTHEKE_VIEW (object);
	g_free (view->priv);
	view->priv = NULL;
	
	if (G_OBJECT_CLASS (parent_class)->finalize)
		G_OBJECT_CLASS (parent_class)->finalize (object);	
}

static void
apotheke_view_load_uri (ApothekeView *view, const char *location)
{
	ApothekeViewPrivate *priv;
	gboolean hide_ignored;

	priv = view->priv;

	if (priv->ad != NULL)
		g_object_unref (priv->ad);

	priv->ad = apotheke_directory_new (location);
	apotheke_directory_create_file_list (priv->ad);

	hide_ignored = (gconf_client_get_int (priv->config, APOTHEKE_CONFIG_HIDE_IGNORED, NULL) == 1);
	if (priv->ui_component) {
		bonobo_ui_component_set_prop (priv->ui_component,
					      "/commands/Hide Ignored Files",
					      "state",
					      hide_ignored ? "1" : "0",
					      NULL);
	}
	apotheke_directory_set_hide_ignored_files (priv->ad, hide_ignored);

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), 
				 GTK_TREE_MODEL (priv->ad));
}


static void
apotheke_view_load_location_callback (NautilusView *nautilus_view, 
				      const char *location,
				      ApothekeView *view)
{
        nautilus_view_report_load_underway (NAUTILUS_VIEW (view));
	apotheke_view_load_uri (view, location);
        nautilus_view_report_load_complete (NAUTILUS_VIEW (view));
}
