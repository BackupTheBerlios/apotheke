#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtktextbuffer.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-i18n.h>
#include "apotheke-client-cvs.h"

static void apotheke_client_cvs_instance_init (ApothekeClientCVS *client);
static void apotheke_client_cvs_class_init (ApothekeClientCVSClass *klass);
static void apotheke_client_cvs_finalize (GObject *object);

struct _ApothekeClientCVSPrivate {
	pid_t         child_pid;
	GtkTextBuffer *console;
};

#define CLIENT_CVS_DEBUG 1

GNOME_CLASS_BOILERPLATE (ApothekeClientCVS, apotheke_client_cvs,
			 GObject, G_TYPE_OBJECT);

static void 
apotheke_client_cvs_instance_init (ApothekeClientCVS *client)
{
	ApothekeClientCVSPrivate *priv;

	priv = g_new0 (ApothekeClientCVSPrivate, 1);
	priv->child_pid = 0;
	priv->console = NULL;

	client->priv = priv;
}

static void 
apotheke_client_cvs_class_init (ApothekeClientCVSClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);

        gobject_class->finalize = apotheke_client_cvs_finalize;

}

static void 
apotheke_client_cvs_finalize (GObject *object)
{
	ApothekeClientCVS *client;
	ApothekeClientCVSPrivate *priv;

	client = APOTHEKE_CLIENT_CVS (object);
	priv = client->priv;

	if (!priv) return;

	if (priv->console)
		g_object_unref (G_OBJECT (priv->console));
	priv->console = NULL;

	g_free (priv);
	client->priv = NULL;
}

ApothekeClientCVS* 
apotheke_client_cvs_new (GtkTextBuffer *console)
{
	ApothekeClientCVS *client;

	client = g_object_new (APOTHEKE_TYPE_CLIENT_CVS, NULL);
	
	if (console != NULL) {
		g_object_ref (console);
		client->priv->console = console;
	}
	
	return client;
}

#define option2string(value, o1, o2) value ? o1 : o2                   


static gchar*
add_cmd_files (gchar *cmd, GList *files)
{
	GList *it;
	gchar *tmp;

	for (it = files; it != NULL; it = it->next) {
		tmp = g_strconcat (cmd, " ", (gchar*) it->data, NULL);
		g_free (cmd);
		cmd = tmp;
	}
	
	return cmd;
}


static gchar*
assemble_status_cmd (ApothekeDirectory *dir, 
		     ApothekeOptionsStatus *options, 
		     GList *files)
{
	gchar *cmd;
       
	cmd = g_strconcat ("cvs -f status ", 
			   option2string (options->verbose, "-v", ""),
			   " ", 
			   option2string (options->recursive, "-R", "-l"),
			   NULL); 
	
	cmd = add_cmd_files (cmd, files);

	return cmd;
}

static gchar*
assemble_diff_cmd (ApothekeDirectory *dir, 
		   ApothekeOptionsDiff *options, 
		   GList *files)
{
	gchar *cmd;
	gchar *revision;

	switch (options->operation) {
	case APOTHEKE_DIFF_COMPARE_LOCAL_REMOTE:
		revision = NULL;
		break;

	case APOTHEKE_DIFF_COMPARE_LOCAL_TAG:
		revision = g_strconcat (option2string (options->first_is_date, "-D", "-r"),
					" ", options->first_tag, NULL);
		break;
	case APOTHEKE_DIFF_COMPARE_TAG_TAG:
		revision = g_strconcat (option2string (options->first_is_date, "-D", "-r"),
					" ", options->first_tag, " ",
					option2string (options->second_is_date, "-D", "-r"),
					" ", options->second_tag, 
					NULL);
		break;
	default:
		revision = NULL;
	}


	cmd = g_strconcat ("cvs -f diff ", 
			   option2string (options->recursive, "-R", "-l"), 
			   " ",
			   option2string (options->include_add_removed_files, "-N", ""),
			   " ", 
			   option2string (options->unified_diff, "-u", ""),
			   " ",
			   option2string (options->whitespaces, "-b", ""),
			   " ",
			   revision,
			   NULL); 

	cmd = add_cmd_files (cmd, files);

	if (revision != NULL)
		g_free (revision);

	return cmd;
}

static gchar*
assemble_commit_cmd (ApothekeDirectory *dir, 
		     ApothekeOptionsCommit *options, 
		     GList *files)
{
	gchar *cmd;

	cmd = g_strconcat ("cvs -f commit ",
			   option2string (options->recursive, "-R", "-l"),
			   " -m \"", options->message, "\"",
			   NULL); 

	cmd = add_cmd_files (cmd, files);

	return cmd;
}

static gchar*
assemble_update_cmd (ApothekeDirectory *dir, 
		     ApothekeOptionsUpdate *options, 
		     GList *files)
{
	gchar *cmd;
	gchar *sticky_char;

	switch (options->sticky_options) {
	case APOTHEKE_UPDATE_STICKY_DONT_CHANGE:
		sticky_char = NULL;
		break;
	case APOTHEKE_UPDATE_STICKY_RESET:
		sticky_char = g_strdup ("-A ");
		break;
	case APOTHEKE_UPDATE_STICKY_DATE:
		sticky_char = g_strconcat ("-D ", options->sticky_tag, NULL);
		break;
	case APOTHEKE_UPDATE_STICKY_TAG:
		sticky_char = g_strconcat ("-r ", options->sticky_tag, NULL);
		break;
	}

	cmd = g_strconcat ("cvs -f update ",
			   option2string (options->recursive,   "-R", "-l"), " ",
			   option2string (options->create_dirs, "-d", ""), " ",
			   sticky_char,
			   NULL);
	
	
	cmd = add_cmd_files (cmd, files);

	if (sticky_char != NULL) {
		g_free (sticky_char);
	}

	return cmd;
}

gboolean 
apotheke_client_cvs_do (ApothekeClientCVS   *client, 
			ApothekeDirectory   *dir,
			ApothekeCommandType type,
			ApothekeOptions     *options,
			GList               *files)
{
	int fd[2];
	pid_t pid;
	FILE *fd_read;
	char *buffer;
	gchar *cmd_line;
	gchar *cvs_cmd;
	GtkTextIter iter;
	gchar *utf_txt;
	int n_bytes_read;
	int count;

	g_return_val_if_fail (APOTHEKE_IS_CLIENT_CVS (client), FALSE);
	g_return_val_if_fail (APOTHEKE_IS_DIRECTORY (dir), FALSE);
	g_return_val_if_fail (options != NULL, FALSE);
	g_return_val_if_fail (options->type == type, FALSE);

	cvs_cmd = NULL;

	switch (type) {
	case APOTHEKE_CMD_STATUS:
		g_print ("status command\n");
		cvs_cmd = assemble_status_cmd (dir, (ApothekeOptionsStatus*) options, 
					       files);
		break;
	case APOTHEKE_CMD_DIFF:
		g_print ("diff command\n");
		cvs_cmd = assemble_diff_cmd (dir, (ApothekeOptionsDiff*) options,
					     files);
		break;

	case APOTHEKE_CMD_COMMIT:
		g_print ("commit command\n");
		cvs_cmd = assemble_commit_cmd (dir, (ApothekeOptionsCommit*) options,
					       files);
		break;

	case APOTHEKE_CMD_UPDATE:
		g_print ("update command\n");
		cvs_cmd = assemble_update_cmd (dir, (ApothekeOptionsUpdate*) options,
					       files);		
		break;
	default:
		cvs_cmd = NULL;
	};

	if (cvs_cmd == NULL) return FALSE;

#ifdef CLIENT_CVS_DEBUG
	g_print ("cvs cmd: %s\n", cvs_cmd);
#endif
	/* print out executed cvs command */
	utf_txt = g_locale_to_utf8 (cvs_cmd, -1, NULL, NULL, NULL);
	gtk_text_buffer_get_end_iter (client->priv->console, &iter);
	gtk_text_buffer_insert (client->priv->console, &iter, utf_txt, -1);
	gtk_text_buffer_insert (client->priv->console, &iter, "\n", -1);
	g_free (utf_txt);

	if (pipe (fd) < 0) {
		g_warning (_("Couldn't open pipe.\n"));
		g_free (cvs_cmd);
		return FALSE;
	};

	client->priv->child_pid = fork ();
	if (client->priv->child_pid < 0) {
		g_warning (_("Couldn't fork process.\n"));
		g_free (cvs_cmd);
		return FALSE;
	}
	else if (client->priv->child_pid > 0) {
		/* parent process */
		close (fd[1]); /* close write side */

		buffer = g_new0 (char, 1024);
		count = 0;

		while (TRUE) {
			n_bytes_read = read (fd[0], buffer, 1024);

			if (n_bytes_read == 0 /* EOF */)
				break;

			if (client->priv->console != NULL) {
				gtk_text_buffer_get_end_iter (client->priv->console,
							      &iter);
				utf_txt = g_locale_to_utf8 (buffer, n_bytes_read, NULL, NULL, NULL);

				gtk_text_buffer_insert (client->priv->console,
							&iter,
							utf_txt, 
							n_bytes_read);
				g_free (utf_txt);
			}

			while (gtk_events_pending ()) 
				gtk_main_iteration ();
		}
		utf_txt = g_locale_to_utf8 (_("*** command end ***"), -1, NULL, NULL, NULL);
		gtk_text_buffer_insert (client->priv->console,
					&iter,
					utf_txt, 
					-1);
		gtk_text_buffer_insert (client->priv->console,
					&iter,
					"\n\n", /* line break */
					-1);
		g_free (utf_txt);
		g_free (buffer);
	}
	else {
	        /* child process */
		close (fd[0]);
		if (fd[1] != STDOUT_FILENO) {
			if (dup2 (fd[1], STDOUT_FILENO) != STDOUT_FILENO)
				g_warning ("Error on dup2\n");
			close (fd[1]);
		}

		cmd_line = g_strconcat ("(cd ", 
					gnome_vfs_get_local_path_from_uri (apotheke_directory_get_uri (dir)), 
					"; ",
					cvs_cmd, ") 2>&1", NULL);

		if (execl ("/bin/sh", "sh", "-c", cmd_line, NULL)< 0) {
			g_warning ("Error on execv.");
			g_free (cmd_line);
			return FALSE;
		}

		g_free (cmd_line);
	}

	g_free (cvs_cmd);
	
	return TRUE;
}

gboolean 
apotheke_client_cvs_abort (ApothekeClientCVS *client)
{
	return FALSE;
}
