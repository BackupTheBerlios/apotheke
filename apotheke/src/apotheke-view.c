#include <config.h>
#include <glib/gtypes.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkliststore.h>
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

#include "apotheke-view.h"
#include "apotheke-directory.h"
#include "apotheke-client-cvs.h"

struct _ApothekeViewPrivate {
	ApothekeDirectory *ad;

	GtkWidget    *event_box;
	
	GtkWidget     *tree_view;
	GtkWidget     *text_view;
	GtkTextBuffer *console;

	ApothekeClientCVS *client;

	gboolean     hide_ignored_files;

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

static void apotheke_view_class_init (ApothekeViewClass *klass);
static void apotheke_view_instance_init (ApothekeView *view);
static void apotheke_view_finalize (GObject *object);
static void apotheke_view_destroy (BonoboObject *object);

static void apotheke_view_load_location_callback (NautilusView *nautilus_view, 
						  const char *location,
						  ApothekeView *view);
static void apotheke_view_load_uri (ApothekeView *view, const char *location);


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
	N_("Flag"),
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

	CREATE_COLUMN (AD_COL_FLAG, 0.5, -1);

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

	view = (ApothekeView*) user_data;

	if (g_strcasecmp (state, "") == 0) {
		/* State goes blank when component is removed; ignore this. */
		return;
	}

	apotheke_directory_set_hide_ignored_files (view->priv->ad,
						   g_strcasecmp (state, "1") == 0);
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
verb_CVS_status_cb (BonoboUIComponent *uic, gpointer user_data,
		    const char *cname)
{
	ApothekeView *view;
	ApothekeOptionsStatus *options;
	GtkTreeSelection *selection;
	GList *files;

	g_print ("CVS STATUS. do it\n");

	view = APOTHEKE_VIEW (user_data);

	options = g_new0 (ApothekeOptionsStatus, 1);
	options->type = APOTHEKE_CMD_STATUS;
	options->compression = 0;
	options->readonly = FALSE;

	options->recursive = FALSE;
	options->verbose = TRUE;
	
	files = NULL;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->priv->tree_view));
	gtk_tree_selection_selected_foreach (selection,
					     add_selected_file_to_list,
					     &files);

	if (!apotheke_client_cvs_do (view->priv->client,
				     view->priv->ad,
				     APOTHEKE_CMD_STATUS,
				     (ApothekeOptions*) options, 
				     files)) 
	{
		g_warning ("Something did go wrong.\n");
	}

	g_list_free (files);
}

static BonoboUIVerb apotheke_verbs[] = {
	BONOBO_UI_VERB ("CVS Status", verb_CVS_status_cb),
	BONOBO_UI_VERB_END
};


static void
merge_bonobo_ui_callback (BonoboControl *control, 
			  gboolean       state, 
			  gpointer       user_data)
{
 	ApothekeView *view;
	BonoboUIComponent *ui_component;

	g_assert (BONOBO_IS_CONTROL (control));
	
	view = APOTHEKE_VIEW (user_data);

	if (state) {
		ui_component = nautilus_view_set_up_ui (NAUTILUS_VIEW (view),
							DATADIR,
							"apotheke-view-ui.xml",
							"apotheke-view");
		
		bonobo_ui_component_add_listener (ui_component, 
						  ID_HIDE_IGNORED_FILES, 
						  hide_ignored_files_state_changed_callback, 
						  view);

		bonobo_ui_component_add_verb_list_with_data (ui_component, 
							     apotheke_verbs,
							     view);
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
construct_ui (ApothekeView *view)
{
	ApothekeViewPrivate *priv;
	GtkWidget *scroll_window;
	GtkWidget *vpane;
	gchar *text;

	priv = view->priv;

	/* set up tree view */
        priv->tree_view = gtk_tree_view_new ();
	set_up_tree_view (GTK_TREE_VIEW (priv->tree_view));
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view)), 
				     GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->tree_view), TRUE);
	g_signal_connect (G_OBJECT (priv->tree_view), "row_activated", 
			  G_CALLBACK (list_activate_callback), view), 
	
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
	gtk_paned_pack1 (GTK_PANED (vpane), scroll_window, TRUE, TRUE);

	/* text console */
	priv->console = gtk_text_buffer_new (NULL);
	text = "Apotheke version " VERSION ", (C) 2002 Jens Finke <jens@triq.net>\n";
	gtk_text_buffer_set_text (priv->console, text, -1);
	priv->text_view = gtk_text_view_new_with_buffer (priv->console);
	scroll_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll_window), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_ALWAYS);
	gtk_container_add (GTK_CONTAINER (scroll_window), priv->text_view);
	gtk_paned_pack2 (GTK_PANED (vpane), scroll_window, TRUE, TRUE);

	gtk_container_add (GTK_CONTAINER (priv->event_box), vpane);
	gtk_widget_show_all (vpane);
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
	g_datalist_init (&priv->icon_cache);

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


#if 0
static GdkPixbuf*
get_file_icon (ApothekeView *view, const char *filename)
{
	int i;
	char *icon_name;
	char *mime_type;
	GdkPixbuf *pixbuf;
	GQuark quark;
	gchar *path;

	if (view->priv->ad == NULL) return NULL;

	path = g_build_filename (view->priv->ad->uri, filename, NULL);

	mime_type = gnome_vfs_get_mime_type (path);
	g_free (path);

	if (mime_type == NULL) {
		g_print ("no mime type info.\n");
		return NULL;
	}

	quark = g_quark_from_string (mime_type);
	pixbuf = (GdkPixbuf*) g_datalist_id_get_data (&view->priv->icon_cache, quark);
	if (pixbuf != NULL) {
		g_print ("found icon for: %s\n", mime_type);
		g_free (mime_type);
		return gdk_pixbuf_ref (pixbuf);
	}

	/* assemble icon name */
	for (i = 0; i < strlen (mime_type); i++) {
		if (mime_type[i] == '/')
			mime_type[i] = '-';
	}
	icon_name = g_strconcat (DATADIR, "/pixmaps/document-icons/gnome-", 
				 mime_type, ".png", NULL);

	pixbuf = NULL;
	/* load and scale icon file */
	if (g_file_test (icon_name, G_FILE_TEST_EXISTS)) {
		GdkPixbuf *tmp;
		int width, height;
		double factor;

		g_print ("load icon: %s\n", icon_name);

		tmp = gdk_pixbuf_new_from_file (icon_name, NULL);
		width = gdk_pixbuf_get_width (tmp);
		height = gdk_pixbuf_get_height (tmp);
		factor = (double) LIST_VIEW_ICON_HEIGHT / (double) height;
		
		pixbuf = gdk_pixbuf_scale_simple (tmp, width * factor,
						  LIST_VIEW_ICON_HEIGHT,
						  GDK_INTERP_BILINEAR);
		g_object_unref (G_OBJECT (tmp));

		g_datalist_id_set_data_full (&view->priv->icon_cache, 
					     quark, 
					     gdk_pixbuf_ref (pixbuf),
					     (GDestroyNotify)gdk_pixbuf_unref);
	}
		
	g_free (icon_name);
	g_free (mime_type);
	
	return pixbuf;
}
#endif

static void
apotheke_view_load_uri (ApothekeView *view, const char *location)
{
	ApothekeViewPrivate *priv;

	priv = view->priv;

	if (priv->ad != NULL)
		g_object_unref (priv->ad);

	priv->ad = apotheke_directory_new (location);
	apotheke_directory_create_file_list (priv->ad);

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
