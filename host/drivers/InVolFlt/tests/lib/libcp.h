#ifndef _LIBCP_H
#define _LIBCP_H

typedef enum CP_OPS {
    CP_NONE = 0,
    CP_CREATE_BARRIER,
    CP_REMOVE_BARRIER,
    CP_TAG,
    CP_COMMIT_TAG,
    CP_REVOKE_TAG,
    CP_FREEZE_FS,
    CP_THAW_FS,
    CP_SN_TAG,
} cp_ops_t;


int create_barrier(char *guid, int timeout);
int remove_barrier(char *guid);
int tagvol(char *dev, char *guid, char *utag, int timeout);
int commit_tag(char *guid);
int revoke_tag(char *guid);
int freeze_fs(char *dev, char *guid, int timeout);
int thaw_fs(char *dev, char *guid);
int single_node_crash_tag(char *dev, char *guid, char *utag, int timeout);

#endif

