#ifndef __APOTHEKE_VIEW_H__
#define __APOTHEKE_VIEW_H__

#include <libnautilus/nautilus-view.h>

#define APOTHEKE_TYPE_VIEW	    (apotheke_view_get_type ())
#define APOTHEKE_VIEW(obj)	    (GTK_CHECK_CAST ((obj), APOTHEKE_TYPE_VIEW, ApothekeView))
#define APOTHEKE_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), APOTHEKE_TYPE_VIEW, ApothekeViewClass))
#define APOTHEKE_IS_VIEW(obj)	    (GTK_CHECK_TYPE ((obj), APOTHEKE_TYPE_VIEW))
#define APOTHEKE_IS_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), APOTHEKE_TYPE_VIEW))

typedef struct _ApothekeView      ApothekeView;
typedef struct _ApothekeViewClass ApothekeViewClass;
typedef struct _ApothekeViewPrivate ApothekeViewPrivate;

struct _ApothekeView {
        NautilusView parent_object;
	ApothekeViewPrivate *priv;
};

struct _ApothekeViewClass {
	NautilusViewClass parent_class;
};

GtkType       apotheke_view_get_type          (void);

#endif /* __APOTHEKE_VIEW_H__ */
