#ifndef _MOUNT__INFOS__H_
#define _MOUNT__INFOS__H_

#include <map>
#include <vector>
#include <list>
#include <iterator>
#include <algorithm>
#include <functional>
#include <string>
#include <sstream>


/* MOUNT INFORMATION */
/* Information for a mountpoint from "/etc/\*tab files and /proc/mounts file */
struct MountInfo
{
    std::string m_MountedResource;/* key has to be mounted resource since may not have dev_t in case of zfs data set.
                                   * can directly compare with major and minor. make sure that we collect atleast 
                                   * all mountpoints so as to report zfs data sets kinds of classess too */
    dev_t m_Devt;                 /* will be 0 for actual devt being 0 or stat fails on mounted resource */
    std::string m_Source;         /* source of information like "/etc/\*tab or "/proc/mounts" */
    bool m_IsMounted;             /* from "/etc/mtab", it is mounted, if source is "/etc/fstab" it is not mounted */
    bool m_IsCacheMountPoint;     /* from drscout.conf, is it cache directory where diff files stored by target. only if mounted */
    bool m_IsSwapFileSystem;      /* Is fs literal "swap". Again need to find swap fs if doing file -s or fstyp */
    bool m_IsSystemMountPoint;    /* system mount points; "/", "/home", "/export/home" and the like */
    std::string m_FileSystem;     /* file system */
    std::string m_MountPoint;     /* mount point */

    MountInfo()
    {
        m_IsMounted = false;
        m_Devt = 0;
        m_IsCacheMountPoint = false;
        m_IsSystemMountPoint = false;
        m_IsSwapFileSystem = false;
    }

    std::string Dump() const
    {
        std::stringstream msg;
        msg << "MountedResource: ";
        msg << m_MountedResource;
        msg << " Devt: ";
        msg << m_Devt;
        msg << " Source: ";
        msg << m_Source;
        msg << " IsMounted: ";
        msg << m_IsMounted;
        msg << " IsCacheMountPoint: ";
        msg << m_IsCacheMountPoint;
        msg << " IsSwapFileSystem: ";
        msg << m_IsSwapFileSystem;
        msg << " IsSystemMountPoint: ";
        msg << m_IsSystemMountPoint;
        msg << " FileSystem: ";
        msg << m_FileSystem;
        msg << " MountPoint: ";
        msg << m_MountPoint;
        msg << ".";

        return msg.str();
    }
};
typedef MountInfo MountInfo_t;

/* multimap because, device is visible in any or all of "/etc/\*tab files. 
 * only one (from /etc/mnttab or /proc/mounts) will have m_IsMounted as true; 
 * Multimap also because in same "/etc/\*tab file, there can be multiple 
 * entries for same resource. true atleast on solaris: 
 * swap    /tmp    tmpfs   xattr,dev=4880002       1271560770
 * swap    /var/run        tmpfs   xattr,dev=4880003       1271560770
*/
typedef std::multimap<std::string, MountInfo_t> MountInfos_t;
typedef std::pair<const std::string, MountInfo_t> MountInfos_pair_t;
typedef MountInfos_t::const_iterator ConstMountInfoIter_t;
typedef MountInfos_t::iterator       MountInfoIter_t;

/* used by all collected volumes for mount points */
class EqMountedResourceDevt: public std::unary_function<MountInfos_pair_t, bool>
{
    dev_t m_Devt;
public:
    /* refrain from using reference for integer types 
     * since it crashes on solaris for 8 byte integers; reason has 
     * to be found */
    explicit EqMountedResourceDevt(const dev_t devt) : m_Devt(devt) { }
    bool operator()(const MountInfos_pair_t &in) const 
    {
        bool beq = false;
        if (in.second.m_Devt && this->m_Devt)
        {
            beq = (in.second.m_Devt == this->m_Devt);
        }
        return beq;
    }
};

#endif /* _MOUNT__INFOS__H_ */
