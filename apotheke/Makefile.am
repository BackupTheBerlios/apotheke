SUBDIRS = src icons po

EXTRA_DIST =  \
	$(schema_DATA) \
        intltool-extract.in \
        intltool-merge.in   \
        intltool-update.in

schemadir = $(sysconfdir)/gconf/schemas
schema_DATA = apotheke.schemas

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_srcdir)/apotheke.schemas
