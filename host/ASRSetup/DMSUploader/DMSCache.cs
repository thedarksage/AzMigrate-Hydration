using System;
using System.Collections.Generic;
using Microsoft.Downloads.Services.Client.DataAccess;

namespace DMSUploader
{

    public class DMSCache
    {
        static DMSCache()
        {
            Cache.Initialize();
        }

        public static Dictionary<Guid, string> primaryProductList = (Cache.GetData("MsProductTechnologies") as Dictionary<Guid, string>);
        public static Dictionary<int, string> downloadTypeList = (Cache.GetData("DownloadTypes") as Dictionary<int, string>);
        public static Dictionary<int, string> msReleaseTypeList = (Cache.GetData("MsReleaseTypes") as Dictionary<int, string>);
        public static Dictionary<int, string> audienceTypesList = (Cache.GetData("AudienceTypes") as Dictionary<int, string>);
        public static Dictionary<int, string> publishGroupList = (Cache.GetData("PublishGroups") as Dictionary<int, string>);
        public static Dictionary<long, string> downloadCategoriesList = (Cache.GetData("DownloadCategories") as Dictionary<long, string>);
        public static Dictionary<string, string> languageList = (Cache.GetData("Languages") as Dictionary<string, string>);
        public static Dictionary<Guid, string> osList = (Cache.GetData("OperatingSystems") as Dictionary<Guid, string>);
    }
}

