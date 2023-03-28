using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class ScoutComponents
    {
        public ConfigurationServer CS { get; private set; }
        public List<ProcessServer> ProcessServers { get; private set; }
        public List<MasterTarget> MasterTargets { get; private set; }
        public List<ScoutAgent> ScoutAgents { get; private set; }

        public ScoutComponents(FunctionResponse listScoutComponentsResponse)
        {
            if (listScoutComponentsResponse != null && listScoutComponentsResponse.ChildList != null && listScoutComponentsResponse.ChildList.Count > 0)
            {
                CS = new ConfigurationServer(listScoutComponentsResponse);

                ParameterGroup processServers = listScoutComponentsResponse.GetParameterGroup("ProcessServers");
                if (processServers != null && processServers.ChildList != null && processServers.ChildList.Count > 0)
                {
                    ProcessServers = new List<ProcessServer>();
                    foreach (var item in processServers.ChildList)
                    {
                        if (item is ParameterGroup)
                        {
                            ProcessServers.Add(new ProcessServer(item as ParameterGroup));
                        }
                    }
                }

                ParameterGroup masterTargets = listScoutComponentsResponse.GetParameterGroup("MasterTargets");
                if (masterTargets != null && masterTargets.ChildList != null && masterTargets.ChildList.Count > 0)
                {
                    MasterTargets = new List<MasterTarget>();
                    foreach (var item in masterTargets.ChildList)
                    {
                        if (item is ParameterGroup)
                        {
                            MasterTargets.Add(new MasterTarget(item as ParameterGroup));
                        }
                    }
                }

                ParameterGroup sourceServers = listScoutComponentsResponse.GetParameterGroup("SourceServers");
                if (sourceServers != null && sourceServers.ChildList != null && sourceServers.ChildList.Count > 0)
                {
                    ScoutAgents = new List<ScoutAgent>();
                    foreach (var item in sourceServers.ChildList)
                    {
                        if (item is ParameterGroup)
                        {
                            ScoutAgents.Add(new ScoutAgent(item as ParameterGroup));
                        }
                    }
                }
            }
        }
    }
}
