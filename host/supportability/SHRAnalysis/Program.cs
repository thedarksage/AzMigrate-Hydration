using System;
using System.Collections.Generic;

using System.IO;
using System.Linq;

namespace Microsoft.Internal.SiteRecovery.SHRAnalysis
{
    class Program
    {

        static IList<string>    mailingList = null;
        static DateTime         startTime;
        static DateTime         endTime;

        static bool ParseArguments(string[] args)
        {
            bool areInputsValid = false;
            for (int idx=0; idx < args.Length; idx++)
            {
                if (args[idx].Equals("-mail", StringComparison.InvariantCultureIgnoreCase))
                {
                    if (idx+1 >= args.Length)
                    {
                        return false;
                    }

                    mailingList = args[idx + 1].Split(',');
                    idx++;
                    areInputsValid = true;
                    continue;
                }

                if (args[idx].Equals("-start", StringComparison.InvariantCultureIgnoreCase) ||
                    args[idx].Equals("-end", StringComparison.InvariantCultureIgnoreCase))
                {
                    if (idx + 1 >= args.Length)
                    {
                        return false;
                    }

                    if (args[idx].Equals("-start", StringComparison.InvariantCultureIgnoreCase))
                    {
                        startTime = DateTime.Parse(args[idx + 1]);
                    }
                    else
                    {
                        endTime = DateTime.Parse(args[idx + 1]);
                    }
                    idx++;
                    areInputsValid = true;
                    continue;
                }

            }


            return areInputsValid;
        }

        static void Main(string[] args)
        {
            DateTime lastWeek = DateTime.Now.AddDays(-(int)DateTime.Now.DayOfWeek - 6);
            startTime = lastWeek;
            endTime = DateTime.Now;

            if (!ParseArguments(args))
            {
                Console.WriteLine("Failed to parse agrgument");
                return;
            }

            /*
            IList<string> recipients = new List<string>()
            {
                "saku@microsoft.com",
                "lamuppa@microsoft.com",
                "arvindp@microsoft.com",
                "arult@microsoft.com"
            };
            */
            try
            {
                var ClientAllRequestErrorMap = SHRAnalyzer.GetSHRData(startTime, endTime, isCustomerJourney: false);
                var ClientlastRequestErrorMap = SHRAnalyzer.GetSHRData(startTime, endTime, isCustomerJourney: true);
                var shrTable = SHRAnalyzer.GetDataTableForSHR(ClientAllRequestErrorMap, ClientlastRequestErrorMap);

                IList<string> attachments = new List<string>();

                Action<string> SaveSHRFiles = attribute =>
                {
                   if (shrTable.ContainsKey(attribute))
                   {
                       string fileName = string.Format("{0}.csv", attribute);
                       string path = Path.Combine(Directory.GetCurrentDirectory(), fileName);
                       if (File.Exists(path))
                       {
                           File.Delete(path);
                       }
                       File.WriteAllText(path, shrTable[attribute]);
                       attachments.Add(path);
                   }
                };

                SaveSHRFiles("CustomerJourney");
                SaveSHRFiles("AllRequests");

                EmailHandler.SendAutomatedSHRAnalysis(mailingList, shrTable["summary"].ToString(),
                    string.Format("Automated SHR analyis from {0} to {1}", startTime.ToShortDateString(),
                                        endTime.ToShortDateString()), attachments);

                Array.ForEach(attachments.ToArray(), delegate (string attachment) { File.Delete(attachment); });
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex);
            }
        }
    }
}
