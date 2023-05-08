using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class ConsistencyTagInfo
    {
        public string TagId { get; private set; }
        public string TagName { get; private set; }
        public ConsistencyTagAccuracy TagAccuracy { get; private set; }
        public string TagApplication { get; private set; }
        // Consitency tag issued time in UTC format
        public DateTime? TagTimestamp { get; private set; }

        public ConsistencyTagInfo(ParameterGroup consistencyTag)
        {
            TagId = consistencyTag.Id;
            TagName = consistencyTag.GetParameterValue("TagName");
            TagAccuracy = Utils.ParseEnum<ConsistencyTagAccuracy>(consistencyTag.GetParameterValue("LatestTagAccuracy"));
            TagApplication = consistencyTag.GetParameterValue("LatestTagApplication");
            TagTimestamp = Utils.UnixTimeStampToDateTime(consistencyTag.GetParameterValue("LatestTagTimeStamp"));            
        }
    }

    public class ConsistencyPoints
    {
        public string ProtectionPlanId { get; private set; }
        public List<ConsistencyTagInfo> ConsistencyPointList { get; private set; }
        public Dictionary<string, List<ConsistencyTagInfo>> ConsistencyPointsOfServers { get; private set; }

        public ConsistencyPoints(ParameterGroup planConsistencyPointsPG)
        {
            ProtectionPlanId = planConsistencyPointsPG.GetParameterValue("ProtectionPlanId");
            ParameterGroup consistencyPointsPG = planConsistencyPointsPG.GetParameterGroup("ConsistencyPoints");            
            ConsistencyPointList = ParseConsistencyPoints(consistencyPointsPG);
            
            ParameterGroup serversPG = planConsistencyPointsPG.GetParameterGroup("Servers");
            if (serversPG != null && serversPG.ChildList != null)
            {
                ConsistencyPointsOfServers = new Dictionary<string, List<ConsistencyTagInfo>>();
                List<ConsistencyTagInfo> serverConsistencyPoints;
                ParameterGroup serverConsistencyPointsPG;
                foreach (var item in serversPG.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        serverConsistencyPointsPG = item as ParameterGroup;
                        serverConsistencyPoints = ParseConsistencyPoints(serverConsistencyPointsPG);
                        if (serverConsistencyPoints != null)
                        {
                            ConsistencyPointsOfServers.Add(serverConsistencyPointsPG.GetParameterValue("HostId"), serverConsistencyPoints);
                        }
                    }
                }
            }
        }

        private List<ConsistencyTagInfo> ParseConsistencyPoints(ParameterGroup consistencyPointsPG)
        {
            List<ConsistencyTagInfo> consistencyPoints = null;
            ConsistencyTagInfo consistencyTagInfo;            
            if (consistencyPointsPG != null && consistencyPointsPG.ChildList != null)
            {                
                consistencyPoints = new List<ConsistencyTagInfo>();                
                foreach (var item in consistencyPointsPG.ChildList)
                {
                    if (item is ParameterGroup)
                    {
                        consistencyTagInfo = new ConsistencyTagInfo(item as ParameterGroup);
                        consistencyPoints.Add(consistencyTagInfo);                        
                    }
                }
            }
            return consistencyPoints;
        }
    }
}
