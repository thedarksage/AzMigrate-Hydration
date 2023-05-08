using System;

namespace DMSUploader
{
    // DMSParameters is a singleton class which gets instanciated once and used in entire flow.
    class DMSParameters
    {
        private static DMSParameters _instance;
        private static Object objLock = new Object();

        // The constructor is protected to avoid any creations
        protected DMSParameters()
        {
        }

        /// <summary>
        /// Returns the Instance of the SetupController
        /// </summary>
        /// <returns></returns>
        public static DMSParameters Instance()
        {
            // Uses lazy initialization.
            if (_instance == null)
            {
                lock (objLock)
                {
                    if (_instance == null)
                    {
                        _instance = new DMSParameters();
                    }
                }
            }
            return _instance;
        }

        //Operation
        private string operation = "CreateDownload";
        public string Operation
        {
            get { return operation; }
            set { operation = value; }
        }

        //DMSRequiredParametersFile
        private string dmsRequiredParametersFile;
        public string DMSRequiredParametersFile
        {
            get { return dmsRequiredParametersFile; }
            set { dmsRequiredParametersFile = value; }
        }

        //DownloadName
        private string downloadName;
        public string DownloadName
        {
            get { return downloadName; }
            set { downloadName = value; }
        }

        //ReleaseId
        private Guid releaseId;
        public Guid ReleaseId
        {
            get { return releaseId; }
            set { releaseId = value; }
        }
    }
}
