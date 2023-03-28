#include "libinclude.h"
#include "involflt.h"
#include "libflt.h"

#define FLT_BARRIER_TIMEOUT 100
#define CP_GUID_LEN         GUID_LEN         

char *default_guid = "abcd";
char default_tag[GUID_LEN];
#define DEFAULT_TAG_NAME "TagTest"

int
create_barrier(char *guid, int timeout)
{
    flt_barrier_create_t barrier;
    int fd = -1;

    bzero(&barrier, sizeof(barrier));
    
    if (!guid)
        guid = default_guid;

    if (!timeout)
        timeout = FLT_BARRIER_TIMEOUT;

    snprintf(barrier.fbc_guid, sizeof(barrier.fbc_guid), "%s", guid);
    barrier.fbc_timeout_ms = timeout;

    return control_ioctl(IOCTL_INMAGE_CREATE_BARRIER_ALL, &barrier);
}

int
remove_barrier(char *guid)
{
    flt_barrier_remove_t barrier;
    
    bzero(&barrier, sizeof(barrier));

    if (!guid)
        guid = default_guid;

    snprintf(barrier.fbr_guid, sizeof(barrier.fbr_guid), "%s", guid);

    return control_ioctl(IOCTL_INMAGE_REMOVE_BARRIER_ALL, &barrier);
}

int
_tagvol(int ioctl_cmd, char *dev, char *guid, char *utag, int timeout)
{
    tag_info_t_v2   taginfo;
    volume_info_t   vol;
    tag_names_t     tags;

    bzero(&taginfo, sizeof(tag_info_t_v2));
    bzero(&vol, sizeof(vol));
    bzero(&tags, sizeof(tags));
    
    if (!guid)
        guid = default_guid;

    snprintf(taginfo.tag_guid, sizeof(taginfo.tag_guid), "%s", guid);

    if (!utag) {
        snprintf(default_tag, GUID_LEN, "%s", DEFAULT_TAG_NAME);
        utag = default_tag;
    }
    taginfo.nr_tags = 1;
    taginfo.tag_names = &tags;
    snprintf(tags.tag_name, sizeof(tags.tag_name), "%s", utag);
    tags.tag_len = strlen(utag);

    if (!dev) {
        taginfo.flags |= TAG_ALL_PROTECTED_VOLUME_IOBARRIER;
        taginfo.nr_vols = 1;
    } else {
        taginfo.nr_vols = 1;
        taginfo.vol_info = &vol;
        strncpy(vol.vol_name, dev, sizeof(vol.vol_name));
        vol.status = 1;
        vol.flags = 0;
    }

    if (timeout)
        taginfo.timeout = timeout;

    return control_ioctl(ioctl_cmd, &taginfo);
}

int
tagvol(char *dev, char *guid, char *utag, int timeout)
{
    return _tagvol(IOCTL_INMAGE_TAG_VOLUME_V2, dev, guid, utag, timeout);
}

int
commit_tag(char *guid)
{
    flt_tag_commit_t commit;

    bzero(&commit, sizeof(commit));

    if (!guid)
        guid = default_guid;

    snprintf(commit.ftc_guid, sizeof(commit.ftc_guid), "%s", guid);
    commit.ftc_flags = TAG_COMMIT;

    return control_ioctl(IOCTL_INMAGE_TAG_COMMIT_V2, &commit);
}

int
revoke_tag(char *guid)
{
    flt_tag_commit_t revoke;
    
    bzero(&revoke, sizeof(revoke));

    if (!guid)
        guid = default_guid;

    snprintf(revoke.ftc_guid, sizeof(revoke.ftc_guid), "%s", guid);
    revoke.ftc_flags = TAG_REVOKE;
    revoke.ftc_flags = 0;

    return control_ioctl(IOCTL_INMAGE_TAG_COMMIT_V2, &revoke);
}

int
freeze_fs(char *dev, char *guid, int timeout)
{
    freeze_info_t finfo;
    volume_info_t vinfo;

    bzero(&finfo, sizeof(finfo));
    bzero(&vinfo, sizeof(vinfo));

    finfo.nr_vols = 1;
    finfo.timeout = timeout;
    
    if (!guid)
        guid = default_guid;
    snprintf(finfo.tag_guid, sizeof(finfo.tag_guid), "%s", guid);

    finfo.vol_info = &vinfo;
    vinfo.status = -1;
    strncpy(vinfo.vol_name, dev, sizeof(vinfo.vol_name));

    return control_ioctl(IOCTL_INMAGE_FREEZE_VOLUME, &finfo);
}

int
thaw_fs(char *dev, char *guid)
{
    thaw_info_t tinfo;
    volume_info_t vinfo;

    bzero(&tinfo, sizeof(tinfo));
    bzero(&vinfo, sizeof(vinfo));

    tinfo.nr_vols = 1;
    
    if (!guid)
        guid = default_guid;
    snprintf(tinfo.tag_guid, sizeof(tinfo.tag_guid), "%s", guid);

    tinfo.vol_info = &vinfo;
    vinfo.status = -1;
    strncpy(vinfo.vol_name, dev, sizeof(vinfo.vol_name));

    return control_ioctl(IOCTL_INMAGE_THAW_VOLUME, &tinfo);
}

int
single_node_crash_tag(char *dev, char *guid, char *utag, int timeout)
{
    return _tagvol(IOCTL_INMAGE_IOBARRIER_TAG_VOLUME, dev, guid, utag, 
                   timeout);
}

