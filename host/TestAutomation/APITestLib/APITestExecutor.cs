using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIHelpers;

namespace InMage.Test.API
{
    public class APITestExecutor
    {
        private BaseAPITest apiTest;
        private List<Section> apiSections = new List<Section>();        
        
        // Section name constants of Configuration file
        public const string SectionCXAPIExecutionConfig = "CXAPIExecutionConfig";
        public const string SectionPlaceholders = "Placeholders";

        // Key name constants of Configuration file
        public const string KeyExecuteAPI = "executeapi";

        public APITestExecutor(BaseAPITest apiTestInstance, string configFilePath)
        {
            Init(apiTestInstance, new ConfigurationReader(configFilePath));            
        }

        public APITestExecutor(BaseAPITest apiTestInstance, ConfigurationReader configurationReader)
        {
            Init(apiTestInstance, configurationReader);
        }

        public Dictionary<string, string> Execute()
        {            
            Dictionary<string, string> apiExecutionStatus = new Dictionary<string, string>();
            JobStatus jobStatus = JobStatus.Unknown;
            bool isExecutionFailed = false;

            if (apiSections.Count == 0)
            {
                Logger.Error("No function name has given for execution or section doesn't exist for the given function.");
            }
            else
            {
                foreach (Section section in apiSections)
                {
                    try
                    {
                        if (isExecutionFailed)
                        {
                            apiExecutionStatus.Add(section.Name, "Not Started");
                            continue;
                        }
                        apiTest.ReplacePlaceholders(section);
                        jobStatus = JobStatus.Unknown;

                        FunctionHandler functionHandler = apiTest.GetFunctionHandler(section);
                        jobStatus = functionHandler(section);
                    }
                    catch (Exception ex)
                    {
                        jobStatus = JobStatus.Failed;
                        Console.WriteLine("Failed to execute {0} API. Error: {1}", section.Name, ex.Message);
                        Logger.Error("Failed to execute API " + section.Name + ". Exception Stack: " + ex);
                    }
                    if (!isExecutionFailed)
                    {
                        switch (jobStatus)
                        {
                            case JobStatus.Timedout:
                                apiExecutionStatus.Add(section.Name, "Timedout");
                                isExecutionFailed = true;
                                break;
                            case JobStatus.Completed:
                                apiExecutionStatus.Add(section.Name, "Succeeded");
                                isExecutionFailed = false;
                                break;
                            default:
                                apiExecutionStatus.Add(section.Name, "Failed");
                                isExecutionFailed = true;
                                break;
                        }
                    }
                }
            }
            return apiExecutionStatus;
        }

        private void Init(BaseAPITest apiTestInstance, ConfigurationReader configurationReader)
        {
            this.apiTest = apiTestInstance;            
            string cxIP = null;
            string cxPort = null;
            string cxProtocol = null;
            string[] apiNamesToExecute = null;
            int asyncFunctionTimeOutInSec = 0;
            Section placeholders = null;

            if (configurationReader != null && configurationReader.Sections != null && configurationReader.Sections.Count > 0)
            {
                Section apiExecutionConfigSection = configurationReader.GetSection(SectionCXAPIExecutionConfig);
                if (apiExecutionConfigSection != null)
                {
                    cxIP = apiExecutionConfigSection.GetKeyValue("cxip");
                    cxPort = apiExecutionConfigSection.GetKeyValue("cxport");
                    cxProtocol = apiExecutionConfigSection.GetKeyValue("cxprotocol");
                    string value = apiExecutionConfigSection.GetKeyValue(KeyExecuteAPI);
                    int.TryParse(apiExecutionConfigSection.GetKeyValue("asyncfunctiontimeout"), out asyncFunctionTimeOutInSec);

                    if (!String.IsNullOrEmpty(value))
                    {
                        apiNamesToExecute = value.Split(new char[] { ',', ' ', ';' }, StringSplitOptions.RemoveEmptyEntries);                        
                    }
                }
                placeholders = configurationReader.GetSection(SectionPlaceholders);
                if (apiNamesToExecute == null)
                {
                    foreach (Section section in configurationReader.Sections)
                    {
                        if (!String.Equals(section.Name, SectionCXAPIExecutionConfig, StringComparison.OrdinalIgnoreCase)
                            && !String.Equals(section.Name, SectionPlaceholders, StringComparison.OrdinalIgnoreCase))
                        {
                            apiSections.Add(section);
                        }
                    }
                }
                else
                {
                    Section section = null;
                    foreach (string sectionName in apiNamesToExecute)
                    {
                        section = configurationReader.GetSection(sectionName);
                        if (section != null)
                        {
                            apiSections.Add(section);
                        }
                    }
                }
            }
            else
            {
                Logger.Error("No section found");
            }
            if (!String.IsNullOrEmpty(cxIP) && !String.IsNullOrEmpty(cxPort) && !String.IsNullOrEmpty(cxProtocol))
            {
                apiTest.SetConfiguration(cxIP, int.Parse(cxPort), cxProtocol);
            }
            apiTest.AsyncFunctionTimeOut = asyncFunctionTimeOutInSec;
            if (placeholders != null)
            {
                apiTest.SetPlaceholders(placeholders);
            }
        }

    }
}
