
INCLUDES =                         \
	-DDATADIR=\""$(datadir)"\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale\"" \
	$(APOTHEKE_CFLAGS)

bin_PROGRAMS = apotheke-view

apotheke_view_SOURCES =        \
	apotheke-main.c        \
	apotheke-view.h	       \
	apotheke-view.c        \
	apotheke-directory.h   \
	apotheke-directory.c   \
	apotheke-file.h	       \
	apotheke-client-cvs.h  \
	apotheke-client-cvs.c  \
	apotheke-options.h

apotheke_view_LDADD = $(APOTHEKE_LIBS)

serverdir = $(libdir)/bonobo/servers
server_in_files = Apotheke_View.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

uidir = $(datadir)/gnome-2.0/ui
ui_in_files = apotheke-view-ui.xml.in
ui_DATA = $(ui_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

EXTRA_DIST = $(ui_in_files) $(server_in_files)