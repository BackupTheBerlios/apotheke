SUBDIRS = src icons po

EXTRA_DIST =  \
	$(schema_DATA) \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	apotheke.spec.in \
	apotheke.spec

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = apotheke.schemas

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_srcdir)/apotheke.schemas

CLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update
