#ifndef __APOTHEKE_CLIENT_CVS_H__
#define __APOTHEKE_CLIENT_CVS_H__

#include <glib-object.h>
#include <gtk/gtktextbuffer.h>

#include "apotheke-directory.h"
#include "apotheke-options.h"

#define APOTHEKE_TYPE_CLIENT_CVS	    (apotheke_client_cvs_get_type ())
#define APOTHEKE_CLIENT_CVS(obj)	    (G_TYPE_CHECK_INSTANCE_CAST ((obj), APOTHEKE_TYPE_CLIENT_CVS, ApothekeClientCVS))
#define APOTHEKE_CLIENT_CVS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), APOTHEKE_TYPE_CLIENT_CVS, ApothekeClientCVSClass))
#define APOTHEKE_IS_CLIENT_CVS(obj)	    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), APOTHEKE_TYPE_CLIENT_CVS))
#define APOTHEKE_IS_CLIENT_CVS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), APOTHEKE_TYPE_CLIENT_CVS))
#define APOTHEKE_CLIENT_CVS_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), APOTHEKE_TYPE_CLIENT_CVS, ApothekeClientCVSClass))

typedef struct _ApothekeClientCVS        ApothekeClientCVS;
typedef struct _ApothekeClientCVSClass   ApothekeClientCVSClass;
typedef struct _ApothekeClientCVSPrivate ApothekeClientCVSPrivate;

struct _ApothekeClientCVS {
	GObject    object;

	ApothekeClientCVSPrivate *priv;
};

struct _ApothekeClientCVSClass {
	GObjectClass  parent_class;
};

GType apotheke_client_cvs_get_type (void);

ApothekeClientCVS* apotheke_client_cvs_new (GtkTextBuffer *console);

gboolean apotheke_client_cvs_do (ApothekeClientCVS *client, 
				 ApothekeDirectory *dir,
				 ApothekeCommandType type,
				 ApothekeOptions *options,
				 GList *files);

gboolean apotheke_client_cvs_abort (ApothekeClientCVS *client);
				 

#endif /* __APOTHEKE_CLIENT_CVS_H__ */
