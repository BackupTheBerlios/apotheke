
INCLUDES =                         \
	-DDATADIR=\""$(datadir)"\" \
	-DGLADE_DIR=\"$(gladedir)\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale\"" \
	$(APOTHEKE_CFLAGS)

libexec_PROGRAMS = apotheke-view

apotheke_view_SOURCES =        \
	apotheke-main.c        \
	apotheke-view.h	       \
	apotheke-view.c        \
	apotheke-directory.h   \
	apotheke-directory.c   \
	apotheke-client-cvs.h  \
	apotheke-client-cvs.c  \
	apotheke-highlight-buffer.c \
	apotheke-highlight-buffer.h \
	apotheke-cvs-entries.c \
	apotheke-cvs-entries.h \
	apotheke-cvs-dialogs.h \
	apotheke-cvs-dialogs.c \
	apotheke-options.h     \
	apotheke-glade.h

apotheke_view_LDADD = $(APOTHEKE_LIBS) $(FAM_LIBS)

serverdir = $(libdir)/bonobo/servers
server_in_files = Apotheke_View.server.in
server_DATA = $(server_in_files:.server.in=.server)
$(server_in_files): $(server_in_files).in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

gladedir   = $(pkgdatadir)/glade
glade_DATA = apotheke.glade

uidir = $(datadir)/gnome-2.0/ui
ui_in_files = apotheke-view-ui.xml.in
ui_DATA = $(ui_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

EXTRA_DIST = $(ui_in_files) $(server_in_files) $(glade_DATA)

CLEANFILES = $(ui_DATA) $(server_DATA) $(server_in_files)
