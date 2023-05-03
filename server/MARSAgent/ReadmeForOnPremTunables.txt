Adding an on premise agent tunable.

There will be 3 types of tunables. T1, T2 and T3.
T1 -> On-Prem agents have set the values for these tunable in the code.
T2 -> On-prem agents want to receive the value of this tunables from RCM. The value is global across all geos.
T3 -> On-prem agents want to receive the value of this tunable from RCM. The tunable value is geo specific.

There will be a file "\src\Service\RcmContract\DataContract\AgentDataContracts\CommonContracts\Settings\AgentTunables.cs", 
this will have one dictionary for each agent 

1. OnPremToAzureReplicationAgentTunables
2. PushInstallServiceTunables
3. ProcessServerTunables
4. RcmProxyTunables
5. SourceAgentTunables

For T1, The value is maintained by on-prem agents within their code base. For example, MT, RcmProxy,
	PushInstall maintain these keys in ServiceTunableName.cs and  value in ServiceTunable.cs.
For T2, In addition to maintaining the value in on-prem code, you also have to add the tunable and
	value in RCM code base in file AgentTunables.cs
For T3, Do what is mentioned for T2 type of tunables and also edit the files mentioned below in RCM code base.

Files to be changed while adding a tunable of type T3.

1. ServiceDefinition.csdef  (To be added in 3 places under 3 roles.)
2. ServiceConfiguration.Cloud.cscfg (To be added in 3 places under 3 roles.)
3. ServiceConfiguration.Local.cscfg (To be added in 3 places under 3 roles.)
4. ApplicationManifest.xml in SF service (Changes at 6 places i.e. Default value and Mapping for 3 roles.)	
	<Parameter Name="RcmWebNode_TunableName" DefaultValue="value" />
	<Parameter Name="RcmWorkerNode_TunableName" DefaultValue="value" />
	<Parameter Name="RcmMonitoringNode_TunableName" DefaultValue="value" />
	<Parameter Name="TunableName" Value="[RcmWebNode_TunableName]" />
	<Parameter Name="TunableName" Value="[RcmWorkerNode_TunableName]" />
	<Parameter Name="TunableName" Value="[RcmMonitoringNode_TunableName]" />
5.	Cloud.xml in SF service (To be added in 3 places under 3 roles.)
6.	Local.xml in SF service (To be added in 3 places under 3 roles.)
7.	Settings.xml (in RcmMonitoringNode project)
8.	Settings.xml (in RcmWebNode project)
9.	Settings.xml (in RcmWorkerNode project)
10.	Production.json

Functionality:
Agents will first read the tunables as sent from RCM. If it does not get the value it will read the 
default as provided in ServiceTunables.cs
RCM will only send tunables present in the dictionary (type of T2 and T3) corresponding to the specific agent. 
While Sending the tunables, RCM will check if the tunable is present in the cofig. 
If present, RCM will send the value read from the config file. Else, RCM will send the tunable value from the dictionary.
