#ifndef __APOTHEKE_OPTIONS_H__
#define __APOTHEKE_OPTIONS_H__

typedef struct _ApothekeOptions         ApothekeOptions;
typedef struct _ApothekeOptionsStatus   ApothekeOptionsStatus;
typedef struct _ApothekeOptionsDiff     ApothekeOptionsDiff;
typedef struct _ApothekeOptionsCommit   ApothekeOptionsCommit;
typedef struct _ApothekeOptionsUpdate   ApothekeOptionsUpdate;

#define APOTHEKE_OPTIONS(o) (((ApothekeOptions*)o))

typedef enum {
	APOTHEKE_CMD_ADD,
	APOTHEKE_CMD_CHECKOUT,
	APOTHEKE_CMD_COMMIT,
	APOTHEKE_CMD_DIFF,
	APOTHEKE_CMD_IMPORT,
	APOTHEKE_CMD_LOG,
	APOTHEKE_CMD_REMOVE,
	APOHTEKE_CMD_RTAG,
	APOTHEKE_CMD_STATUS,
	APOHTEKE_CMD_TAG,
	APOTHEKE_CMD_UPDATE
} ApothekeCommandType;


struct _ApothekeOptions {
	ApothekeCommandType  type;
	int                  compression;
};

struct _ApothekeOptionsStatus {
	ApothekeOptions      options;
	
	gboolean recursive;
	gboolean verbose;
};

typedef enum {
	APOTHEKE_DIFF_COMPARE_LOCAL_REMOTE,
	APOTHEKE_DIFF_COMPARE_LOCAL_TAG,
	APOTHEKE_DIFF_COMPARE_TAG_TAG
} ApothekeDiffCompareType;

struct _ApothekeOptionsDiff {
	ApothekeOptions     options;

	ApothekeDiffCompareType operation;
	gchar *first_tag;
	gboolean first_is_date;
	gchar *second_tag;
	gboolean second_is_date;

	gboolean recursive;
	gboolean include_add_removed_files;
	gboolean unified_diff;
	gboolean whitespaces;
};

struct _ApothekeOptionsCommit {
	ApothekeOptions     options;

	gboolean recursive;
	gchar *message;
};

typedef enum {
	APOTHEKE_UPDATE_STICKY_DONT_CHANGE,
	APOTHEKE_UPDATE_STICKY_RESET,
	APOTHEKE_UPDATE_STICKY_DATE,
	APOTHEKE_UPDATE_STICKY_TAG
} ApothekeUpdateStickyOptions;

struct _ApothekeOptionsUpdate {
	ApothekeOptions    options;

	ApothekeUpdateStickyOptions sticky_options;
	gchar *sticky_tag;   /* only valid for STICKY_DATE and STICKY_TAG */
	gboolean recursive;
	gboolean create_dirs;
};

#endif /* __APOTHEKE_OPTIONS_H__ */
