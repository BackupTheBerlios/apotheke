#ifndef __APOTHEKE_OPTIONS_H__
#define __APOTHEKE_OPTIONS_H__

typedef struct _ApothekeOptions       ApothekeOptions;
typedef struct _ApothekeOptionsStatus ApothekeOptionsStatus;
typedef struct _ApothekeOptionsDiff   ApothekeOptionsDiff;

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
	ApothekeCommandType  type;
	int                  compression;
	
	gboolean recursive;
	gboolean verbose;
};

struct _ApothekeOptionsDiff {
	ApothekeCommandType  type;
	int                  compression;

	gboolean recursive;
	gboolean include_add_removed_files;
	gboolean unified_diff;
};

#endif /* __APOTHEKE_OPTIONS_H__ */
