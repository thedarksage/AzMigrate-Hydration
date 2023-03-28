using System;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.Linq;


using Microsoft.TeamFoundation.WorkItemTracking.WebApi;
using Microsoft.TeamFoundation.WorkItemTracking.WebApi.Models;
using Microsoft.VisualStudio.Services.Common;
using Microsoft.VisualStudio.Services.WebApi;
using Microsoft.VisualStudio.Services.Client;

namespace Microsoft.Internal.SiteRecovery.SHRAnalysis
{
    class VSSHelper
    {
        static public IDictionary<string, IList<WorkItem>> GetWorkItems(ICollection<string> titles)
        {
            IDictionary<string, IList<WorkItem>> TitleWorkItems = new Dictionary<string, IList<WorkItem>>();

            Uri orgUrl = new Uri("https://dev.azure.com/msazure");
            using (VssConnection connection = new VssConnection(orgUrl, new VssClientCredentials()))
            {
                var r = QueryOpenBugs(connection, "one", titles);
                r.Wait();

                foreach (var title in titles)
                {
                    if (!TitleWorkItems.ContainsKey(title))
                    {
                        TitleWorkItems.Add(title, new List<WorkItem>());
                    }

                    foreach (var item in r.Result)
                    {
                        string Id = item.Fields["System.Id"].ToString();
                        string Title = item.Fields["System.Title"].ToString();
                        string state = item.Fields["System.State"].ToString();

                        if (Title.IndexOf(title, 0, StringComparison.CurrentCultureIgnoreCase) != -1)
                        {
                            TitleWorkItems[title].Insert(0, item);
                        }
                    }
                }
            }
            return TitleWorkItems;
        }

        static public async Task<IList<WorkItem>> QueryOpenBugs(VssConnection connection, string project, ICollection<string> titles)
        {
            string query = "(" + String.Join(" OR ", titles.Select(x => "[System.Title] Contains '" + x + "'")) + ")";

            string query1 = string.Format("Select [Id] " +
                        "From WorkItems " +
                        "Where [Work Item Type] = 'Bug' " +
                        "And [System.TeamProject] = '" + project + "' " +
                        "AND [System.AreaPath] UNDER '" + @"One\Azure Compute\Azure Site Recovery" + "' " +
                        "And {0} " +
                        "Order By [State] Asc, [Changed Date] Desc", query);

            // create a wiql object and build our query
            var wiql = new Wiql()
            {
                // NOTE: Even if other columns are specified, only the ID & URL will be available in the WorkItemReference
                Query = query1,
            };

            Console.WriteLine("Qeury: {0}", wiql.Query);
            // create instance of work item tracking http client
            using (var httpClient = connection.GetClient<WorkItemTrackingHttpClient>())
            {
                // execute the query to get the list of work items in the results
                var result = await httpClient.QueryByWiqlAsync(wiql).ConfigureAwait(false);
                var ids = result.WorkItems.Select(item => item.Id).ToArray();

                // some error handling
                if (ids.Length == 0)
                {
                    return Array.Empty<WorkItem>();
                }

                // build a list of the fields we want to see
                var fields = new[] { "System.Id", "System.Title", "System.State", "System.AreaPath", "System.AssignedTo" };

                // get work items for the ids found in query
                return await httpClient.GetWorkItemsAsync(ids, fields, result.AsOf).ConfigureAwait(false);
            }
        }
    };
}