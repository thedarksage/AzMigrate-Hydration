#ifndef SNAPSHOST_CONFIG_H
#define SNAPSHOT_CONFIG_H

class SnapShotConfig ;
typedef boost::shared_ptr<SnapShotConfig> SnapShotConfigPtr ;
class SnapShotConfig
{
    static SnapShotConfigPtr m_snapshotconfigPtr ;
    void loadSnapShotGroups() ;
public:
    void UpdateSnapshostProgress(const std::string& snapShotId,
                                 const std::string& percentComplete,
                                 const std::string& rpoint) ;
    void UpdateSnapshotProgress(const std::string& vsnapId, 
                                const std::string& percentComplete, 
                                const std::string& rpoint) ;

    static SnapShotConfigPtr GetInstance() ;
} ;
#endif