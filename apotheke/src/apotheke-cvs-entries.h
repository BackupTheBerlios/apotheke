#ifndef __APOTHEKE_CVS_ENTRIES_H__
#define __APOTHEKE_CVS_ENTRIES_H__

enum {
	CVS_ENTRIES_DIR,
	CVS_ENTRIES_FILENAME,
	CVS_ENTRIES_REVISION,
	CVS_ENTRIES_DATE,
	CVS_ENTRIES_OPTION,
	CVS_ENTRIES_TAG
};

typedef void (* ApothekeCVSEntriesCallback) (char **entry, gpointer data);


char** apotheke_cvs_entries_get_entry (char* uri, char* filename);

void  apotheke_cvs_entries_for_each (char *uri, ApothekeCVSEntriesCallback callback, gpointer data);


#endif  /* __APOTHEKE_CVS_ENTRIES_H__ */
