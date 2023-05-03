using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Xml;
using System.Xml.Serialization;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi.XmlElements
{
    // NOTE-SanKumar-1904: XmlSerializer expects the classes that are serialized
    // to be public types
    #region XmlElements

    [XmlRoot("Request", Namespace = null)]
    public class RequestElement
    {
        [XmlAttribute]
        public string Id;

        [XmlAttribute]
        public string Version;

        public HeaderElement Header;

        public BodyElement Body;
    }

    public class AuthenticationElement
    {
        public string AuthMethod;
        public string AccessKeyID;
        public string AccessSignature;
    }

    public class HeaderElement
    {
        public AuthenticationElement Authentication;
    }

    public class ParameterElement
    {
        [XmlAttribute]
        public string Name;

        [XmlAttribute]
        public string Value;
    }

    [XmlType("FunctionRequest")]
    public class FunctionRequestElement
    {
        [XmlAttribute]
        public string Name;

        [XmlAttribute]
        public string Id;

        [XmlElement("ParameterGroup")]
        public List<ParameterGroupElement> ParameterGroupList;

        [XmlElement("Parameter")]
        public List<ParameterElement> ParameterList;
    }

    [XmlRoot("Request", Namespace = null)]
    public class UpdatePSStatisticsRequestElement
    {
        [XmlAttribute]
        public string Id;

        [XmlAttribute]
        public string Version;

        [XmlElement("ParameterGroup")]
        public List<ParameterGroupElement> ParameterGroupList;

        [XmlElement("Parameter")]
        public List<ParameterElement> ParameterList;
    }

    public class BodyElement
    {
        public FunctionRequestElement FunctionRequest;
    }

    [XmlRoot("Response")]
    public class ResponseElement
    {
        [XmlAttribute]
        public string ID;

        [XmlAttribute]
        public string Version;

        [XmlAttribute]
        public string Returncode;

        [XmlAttribute]
        public string Message;

        [XmlElement]
        public ResponseBodyElement Body;

        [XmlIgnore]
        private bool m_parsed;

        [XmlIgnore]
        private static readonly XmlSerializer xs = new XmlSerializer(typeof(ResponseElement));

        public static ResponseElement Parse(Stream stream)
        {
            var toRet = xs.Deserialize(stream) as ResponseElement;
            toRet.m_parsed = true;
            return toRet;
        }

        public void EnsureSuccess()
        {
            if (!m_parsed)
            {
                throw new InvalidOperationException(
                    "This method can be invoked only on a parsed object");
            }

            if (this.Returncode != "0")
            {
                throw new Exception(FormattableString.Invariant(
                    $"Failure received in response of request {this.ID} ({this.Version}) : ({this.Returncode}) {this.Message}"));
            }

            this.Body?.EnsureSuccess();
        }

        public override string ToString()
        {
            return $"ID : {ID}, Version : {Version}, Returncode : {Returncode}, Message : {Message}";
        }
    }

    public class ResponseBodyElement
    {
        [XmlElement("Function")]
        public List<FunctionElement> FunctionList;

        public void EnsureSuccess()
        {
            // TODO-SanKumar-1904: Should we throw here? Is there any case,
            // where the functions inside body would be empty?
            if (this.FunctionList == null || this.FunctionList.Count == 0)
                return;

            foreach (var currFunc in this.FunctionList)
            {
                if (currFunc.Returncode != "0")
                {
                    throw new Exception(FormattableString.Invariant(
                        $"Failure received in response of function {currFunc.Name} ({currFunc.Id}) : ({currFunc.Returncode}) {currFunc.Message}"));
                }
            }
        }
    }

    public class FunctionElement
    {
        [XmlAttribute]
        public string Name;

        [XmlAttribute]
        public string Id;

        [XmlAttribute]
        public string Returncode;

        [XmlAttribute]
        public string Message;

        [XmlElement("FunctionResponse")]
        public FunctionResponseElement FunctionResponse;
    }

    public class FunctionResponseElement
    {
        [XmlAttribute]
        public string Id;

        // TODO-SanKumar-1903: How to make this as a set? Or should this be
        // an intermediate object/xml step-by-step serializer forming hashset?
        [XmlElement("ParameterGroup")]
        public List<ParameterGroupElement> ParameterGroupList;

        [XmlElement("Parameter")]
        public List<ParameterElement> ParameterList;
    }

    public class ParameterGroupElement
    {
        [XmlAttribute]
        public string Id;

        // TODO-SanKumar-1903: How to make this as a set? Or should this be
        // an intermediate object/xml step-by-step serializer forming hashset?
        [XmlElement("ParameterGroup")]
        public List<ParameterGroupElement> ParameterGroupList;

        [XmlElement("Parameter")]
        public List<ParameterElement> ParameterList;
    }

    #endregion XmlElements

    internal class UpdateDBItem
    {
        public string QueryType;
        public string TableName;
        public string FieldNames;
        public string Condition;
        public string AdditionalInfo;

        public ParameterGroupElement GetParameterGroupElement(string id)
        {
            if (string.IsNullOrWhiteSpace(QueryType) || string.IsNullOrWhiteSpace(TableName))
                throw new InvalidOperationException();

            return new ParameterGroupElement()
            {
                Id = id,
                ParameterList = new List<ParameterElement>()
                    {
                        new ParameterElement() { Name = "queryType", Value = QueryType},
                        new ParameterElement() { Name = "tableName", Value = TableName},
                        new ParameterElement() { Name = "fieldNames", Value = FieldNames},
                        new ParameterElement() { Name = "condition", Value = Condition},
                        new ParameterElement() { Name = "additionalInfo", Value = AdditionalInfo}
                    }
            };
        }
    }

    internal class Common
    {
        private static readonly XmlSerializer xs = new XmlSerializer(typeof(RequestElement));
        private static readonly XmlSerializerNamespaces xns = new XmlSerializerNamespaces(new[] { new XmlQualifiedName(string.Empty) });

        public static HeaderElement BuildHeader(string hostId, string hmacSignature)
        {
            return new HeaderElement()
            {
                Authentication = new AuthenticationElement()
                {
                    AccessKeyID = hostId,
                    AccessSignature = hmacSignature,
                    AuthMethod = ProcessServerCSApiStubsImpl.CS_AUTH_METHOD
                }
            };
        }

        public static void XmlSerializeRequest(TextWriter tw, string requestId, HeaderElement header, BodyElement body)
        {
            var request = new RequestElement()
            {
                Id = requestId,
                Version = ProcessServerCSApiStubsImpl.CSApiVersion.CURRENT,
                Header = header,
                Body = body
            };

            xs.Serialize(tw, request, xns);
        }
    }

    internal interface ICSApiReqBuilder_TextWriter
    {
        void GenerateXml(TextWriter tw, string requestId, string hostId, string hmacSignature);
    }

    internal class UpdateDBRequest : ICSApiReqBuilder_TextWriter
    {
        private readonly IReadOnlyCollection<UpdateDBItem> m_updateDBItems;

        public UpdateDBRequest(IEnumerable<UpdateDBItem> updateDBItems)
        {
            m_updateDBItems = updateDBItems.ToList().AsReadOnly();
        }

        public void GenerateXml(TextWriter tw, string requestId, string hostId, string hmacSignature)
        {
            const string FnReqId1 = "FnReqId1";

            var body = new BodyElement()
            {
                FunctionRequest = new FunctionRequestElement()
                {
                    Name = ProcessServerCSApiStubsImpl.FunctionName.UPDATE_DB,
                    Id = FnReqId1,
                    ParameterGroupList = m_updateDBItems.Select((currDbItem, ind) =>
                        currDbItem.GetParameterGroupElement("Id" + ind.ToString(CultureInfo.InvariantCulture))).ToList()
                }
            };

            Common.XmlSerializeRequest(tw, requestId, Common.BuildHeader(hostId, hmacSignature), body);
        }
    }

    internal class GetPSSettingsRequest : ICSApiReqBuilder_TextWriter
    {
        public void GenerateXml(TextWriter tw, string requestId, string hostId, string hmacSignature)
        {
            const string FnReqId1 = "FnReqId1";

            var body = new BodyElement()
            {
                FunctionRequest = new FunctionRequestElement()
                {
                    Name = ProcessServerCSApiStubsImpl.FunctionName.GET_PS_SETTINGS,
                    Id = FnReqId1,
                    ParameterList = new List<ParameterElement>
                    {
                        new ParameterElement() { Name = "HostId", Value = hostId }
                    }
                }
            };

            Common.XmlSerializeRequest(tw, requestId, Common.BuildHeader(hostId, hmacSignature), body);
        }
    }

    internal class UpdatePSStatisticsPerformanceRequest : ICSApiReqBuilder_TextWriter
    {
        private string m_psVersion;
        private ProcessServerStatistics m_psStatistics;

        private static readonly XmlSerializer s_serializer
            = new XmlSerializer(typeof(UpdatePSStatisticsRequestElement));

        private static readonly XmlSerializerNamespaces s_xmlNamespaces
            = new XmlSerializerNamespaces(new[] { new XmlQualifiedName(string.Empty) });

        public UpdatePSStatisticsPerformanceRequest(string psVersion, ProcessServerStatistics statistics)
        {
            m_psVersion = psVersion;
            m_psStatistics = statistics.DeepCopy();
        }

        public void GenerateXml(TextWriter tw, string requestId, string hostId, string hmacSignature)
        {
            string GetPercentage(decimal actual, decimal total)
            {
                if (total == 0)
                    throw new InvalidDataException("Total can't be 0");

                // Adding a percentage symbol multiplies the value by 100,
                // before printing. So, there's no explicit multiplication by 100.
                return (actual / total).ToString("0.##%", CultureInfo.InvariantCulture);
            }

            const string unknownVal = "-1";

            var request = new UpdatePSStatisticsRequestElement()
            {
                Id = requestId,
                Version = ProcessServerCSApiStubsImpl.CSApiVersion.CURRENT,

                ParameterList = new List<ParameterElement>
                {
                    new ParameterElement() { Name = "PSGuid", Value = hostId },
                    new ParameterElement() { Name = "PSVersion", Value = m_psVersion }
                },
                ParameterGroupList = new List<ParameterGroupElement>
                {
                    new ParameterGroupElement()
                    {
                        Id = "SystemPerformance",
                        ParameterGroupList = new List<ParameterGroupElement>
                        {
                            new ParameterGroupElement()
                            {
                                Id = "SystemLoad",
                                ParameterList = new List<ParameterElement>()
                                {
                                    new ParameterElement()
                                    {
                                        Name = "SystemLoad",
                                        Value = m_psStatistics.SystemLoad.ProcessorQueueLength.
                                                ToString(CultureInfo.InvariantCulture)
                                    },
                                    new ParameterElement()
                                    {
                                        Name = "Status",
                                        Value = ((int)m_psStatistics.SystemLoad.Status).
                                                ToString(CultureInfo.InvariantCulture)
                                    }
                                }
                            },
                            new ParameterGroupElement()
                            {
                                Id = "CPULoad",
                                ParameterList = new List<ParameterElement>
                                {
                                    new ParameterElement()
                                    {
                                        Name = "CPULoad",
                                        // Since the value is already in percentage, total is 100
                                        Value = (m_psStatistics.CpuLoad.Percentage != -1) ?
                                                    GetPercentage(m_psStatistics.CpuLoad.Percentage, 100) :
                                                    unknownVal
                                    },
                                    new ParameterElement()
                                    {
                                        Name = "Status",
                                        Value = ((int)m_psStatistics.CpuLoad.Status).
                                                ToString(CultureInfo.InvariantCulture)
                                    }
                                }
                            },
                            new ParameterGroupElement()
                            {
                                Id = "MemoryUsage",
                                ParameterList = new List<ParameterElement>
                                {
                                    new ParameterElement()
                                    {
                                        Name = "TotalMemory",
                                        // Setting Used to -1 for scrubbing, if Total is -1
                                        Value = (m_psStatistics.MemoryUsage.Used != -1) ?
                                                    m_psStatistics.MemoryUsage.Total.ToString(CultureInfo.InvariantCulture) :
                                                    unknownVal
                                    },
                                    new ParameterElement()
                                    {
                                        Name = "UsedMemory",
                                        // Setting Total to -1 for scrubbing, if Used is -1
                                        Value = (m_psStatistics.MemoryUsage.Total != -1) ?
                                                    m_psStatistics.MemoryUsage.Used.ToString(CultureInfo.InvariantCulture) :
                                                    unknownVal
                                    },
                                    new ParameterElement()
                                    {
                                        Name = "AvailableMemory",
                                        // Setting available memory to -1, if any one of the value in the MemoryUsage set is -1
                                        Value = (m_psStatistics.MemoryUsage.Total != -1 && m_psStatistics.MemoryUsage.Used != -1) ?
                                                    (m_psStatistics.MemoryUsage.Total - m_psStatistics.MemoryUsage.Used).ToString(CultureInfo.InvariantCulture) :
                                                    unknownVal
                                    },
                                    new ParameterElement()
                                    {
                                        Name = "MemoryUsage",
                                        // Setting MemoryUsage percentage to -1, if any one of the value in the MemoryUsage set is -1
                                        Value = (m_psStatistics.MemoryUsage.Used != -1 && m_psStatistics.MemoryUsage.Total != -1) ?
                                                    GetPercentage(m_psStatistics.MemoryUsage.Used, m_psStatistics.MemoryUsage.Total) :
                                                    unknownVal
                                    },
                                    new ParameterElement()
                                    {
                                        Name = "Status",
                                        Value = ((int)m_psStatistics.MemoryUsage.Status).
                                                ToString(CultureInfo.InvariantCulture)
                                    }
                                }
                            },
                            new ParameterGroupElement()
                            {
                                Id = "FreeSpace",
                                ParameterList = new List<ParameterElement>
                                {
                                    new ParameterElement()
                                    {
                                        Name = "TotalSpace",
                                        // if Used is -1 in the InstallVolumeSpace set, setting Total to -1 for scrubbing
                                        Value = (m_psStatistics.InstallVolumeSpace.Used != -1) ?
                                                    m_psStatistics.InstallVolumeSpace.Total.ToString(CultureInfo.InvariantCulture) :
                                                    unknownVal
                                    },
                                    new ParameterElement()
                                    {
                                        Name = "AvailableSpace",
                                        // Setting available space to -1, if any one of the value in the InstallVolumeSpace set is -1
                                        Value = (m_psStatistics.InstallVolumeSpace.Total != -1 && m_psStatistics.InstallVolumeSpace.Used != -1) ?
                                                    (m_psStatistics.InstallVolumeSpace.Total - m_psStatistics.InstallVolumeSpace.Used).ToString(CultureInfo.InvariantCulture) :
                                                    unknownVal
                                    },
                                    new ParameterElement()
                                    {
                                        Name = "UsedSpace",
                                        // if Total is -1 in the InstallVolumeSpace set, setting Used to -1 for scrubbing
                                        Value = (m_psStatistics.InstallVolumeSpace.Total != -1) ?
                                                    m_psStatistics.InstallVolumeSpace.Used.ToString(CultureInfo.InvariantCulture) :
                                                    unknownVal
                                    },
                                    new ParameterElement()
                                    {
                                        Name = "FreeSpace",
                                        // Setting FreeSpace percentage to -1, if any one of the value in the InstallVolumeSpace set is -1
                                        Value = (m_psStatistics.InstallVolumeSpace.Total != -1 && m_psStatistics.InstallVolumeSpace.Used != -1) ?
                                                    GetPercentage(m_psStatistics.InstallVolumeSpace.Total - m_psStatistics.InstallVolumeSpace.Used, m_psStatistics.InstallVolumeSpace.Total) :
                                                    unknownVal
                                    },
                                    new ParameterElement()
                                    {
                                        Name = "Status",
                                        Value = ((int)m_psStatistics.InstallVolumeSpace.Status).
                                                ToString(CultureInfo.InvariantCulture)
                                    }
                                }
                            },
                            new ParameterGroupElement()
                            {
                                Id = "Throughput",
                                ParameterList = new List<ParameterElement>
                                {
                                    new ParameterElement()
                                    {
                                        Name = "UploadPendingData",
                                        Value = m_psStatistics.Throughput.UploadPendingData.
                                                ToString(CultureInfo.InvariantCulture)
                                    },
                                    new ParameterElement()
                                    {
                                        Name = "Throughput",
                                        // Setting Throughput to -1, if uploadpendingdata=-1 for scrubbing
                                        Value = (m_psStatistics.Throughput.UploadPendingData != -1) ?
                                                    m_psStatistics.Throughput.ThroughputBytesPerSec.ToString(CultureInfo.InvariantCulture) :
                                                    unknownVal
                                    },
                                    new ParameterElement()
                                    {
                                        Name = "Status",
                                        Value = ((int)m_psStatistics.Throughput.Status).
                                                ToString(CultureInfo.InvariantCulture)
                                    }
                                }
                            }
                        }
                    }
                }
            };

            s_serializer.Serialize(tw, request, s_xmlNamespaces);
        }
    }

    internal class UpdatePSStatisticsServicesRequest : ICSApiReqBuilder_TextWriter
    {
        private string m_psVersion;
        private ProcessServerStatistics m_psStatistics;

        private static readonly XmlSerializer s_serializer
            = new XmlSerializer(typeof(UpdatePSStatisticsRequestElement));

        private static readonly XmlSerializerNamespaces s_xmlNamespaces
            = new XmlSerializerNamespaces(new[] { new XmlQualifiedName(string.Empty) });

        public UpdatePSStatisticsServicesRequest(string psVersion, ProcessServerStatistics statistics)
        {
            m_psVersion = psVersion;
            m_psStatistics = statistics.DeepCopy();
        }

        public void GenerateXml(TextWriter tw, string requestId, string hostId, string hmacSignature)
        {
            var request = new UpdatePSStatisticsRequestElement()
            {
                Id = requestId,
                Version = ProcessServerCSApiStubsImpl.CSApiVersion.CURRENT,

                ParameterList = new List<ParameterElement>
                {
                    new ParameterElement() { Name = "PSGuid", Value = hostId },
                    new ParameterElement() { Name = "PSVersion", Value = m_psVersion }
                },
                ParameterGroupList = new List<ParameterGroupElement>
                {
                    new ParameterGroupElement()
                    {
                        Id = "SystemServices",
                        ParameterList = m_psStatistics.Services?.Select(currSvc => new ParameterElement()
                        {
                            Name = currSvc.ServiceName,
                            Value = currSvc.NumberOfInstances.ToString(CultureInfo.InvariantCulture)
                        }).ToList()
                    }
                }
            };

            s_serializer.Serialize(tw, request, s_xmlNamespaces);
        }
    }

    internal class RegisterProcessServerRequest : ICSApiReqBuilder_TextWriter
    {
        private readonly string m_hostInfo;

        private readonly string m_networkInfo;

        public RegisterProcessServerRequest(string hostInfo, string networkInfo)
        {
            if (string.IsNullOrWhiteSpace(hostInfo))
                throw new ArgumentNullException(nameof(hostInfo));

            if (string.IsNullOrWhiteSpace(networkInfo))
                throw new ArgumentNullException(nameof(networkInfo));

            m_hostInfo = hostInfo;

            m_networkInfo = networkInfo;
        }

        public void GenerateXml(TextWriter tw, string requestId, string hostId, string hmacSignature)
        {
            var body = new BodyElement()
            {
                FunctionRequest = new FunctionRequestElement()
                {
                    Name = ProcessServerCSApiStubsImpl.FunctionName.REGISTER_HOST,
                    ParameterGroupList = new List<ParameterGroupElement>
                    {
                        new ParameterGroupElement()
                        {
                            Id = "RegisterHost",
                            ParameterList = new List<ParameterElement>
                            {
                                new ParameterElement() { Name = "host_info", Value = m_hostInfo },
                                new ParameterElement() { Name = "network_info", Value = m_networkInfo }
                            }
                        }
                    }
                }
            };

            Common.XmlSerializeRequest(tw, requestId, Common.BuildHeader(hostId, hmacSignature), body);
        }
    }

    internal class ReportPSEventsRequest : ICSApiReqBuilder_TextWriter
    {
        private readonly string m_eventInfo;

        public ReportPSEventsRequest(string eventInfo)
        {
            if (string.IsNullOrWhiteSpace(eventInfo))
                throw new ArgumentNullException(nameof(eventInfo));

            m_eventInfo = eventInfo;
        }

        public void GenerateXml(TextWriter tw, string requestId, string hostId, string hmacSignature)
        {
            var body = new BodyElement()
            {
                FunctionRequest = new FunctionRequestElement()
                {
                    Name = ProcessServerCSApiStubsImpl.FunctionName.ADD_ERROR_MESSAGE,
                    ParameterGroupList = new List<ParameterGroupElement>
                    {
                        new ParameterGroupElement()
                        {
                            Id = "addErrorMessage",
                            ParameterList = new List<ParameterElement>
                            {
                                new ParameterElement() { Name = "alert_info", Value = m_eventInfo }
                            }
                        }
                    }
                }
            };

            Common.XmlSerializeRequest(tw, requestId, Common.BuildHeader(hostId, hmacSignature), body);
        }
    }

    internal class SetResyncFlagRequest : ICSApiReqBuilder_TextWriter
    {
        private readonly string m_sourceHostId;

        private readonly string m_sourceDeviceName;

        private readonly string m_destHostId;

        private readonly string m_destDeviceName;

        private readonly string m_resyncReasonCode;

        private readonly string m_detectionTime;

        private readonly string m_hardlinkFromFile;

        private readonly string m_hardlinkToFile;

        public SetResyncFlagRequest(string hostId, string deviceName, string destHostId, string destDeviceName,
            string resyncReasonCode, string detectionTime, string hardlinkFromFile, string hardlinkToFile)
        {
            if (string.IsNullOrWhiteSpace(hostId))
                throw new ArgumentNullException(nameof(hostId));

            if (string.IsNullOrWhiteSpace(deviceName))
                throw new ArgumentNullException(nameof(deviceName));

            if (string.IsNullOrWhiteSpace(destHostId))
                throw new ArgumentNullException(nameof(destHostId));

            if (string.IsNullOrWhiteSpace(destDeviceName))
                throw new ArgumentNullException(nameof(destDeviceName));

            if (string.IsNullOrWhiteSpace(resyncReasonCode))
                throw new ArgumentNullException(nameof(resyncReasonCode));

            if (string.IsNullOrWhiteSpace(detectionTime))
                throw new ArgumentNullException(nameof(detectionTime));

            if (string.IsNullOrWhiteSpace(hardlinkFromFile))
                throw new ArgumentNullException(nameof(hardlinkFromFile));

            if (string.IsNullOrWhiteSpace(hardlinkToFile))
                throw new ArgumentNullException(nameof(hardlinkToFile));

            m_sourceHostId = hostId;
            m_sourceDeviceName = deviceName;
            m_destHostId = destHostId;
            m_destDeviceName = destDeviceName;
            m_resyncReasonCode = resyncReasonCode;
            m_detectionTime = detectionTime;
            m_hardlinkFromFile = hardlinkFromFile;
            m_hardlinkToFile = hardlinkToFile;
    }

        public void GenerateXml(TextWriter tw, string requestId, string hostId, string hmacSignature)
        {
            var body = new BodyElement()
            {
                FunctionRequest = new FunctionRequestElement()
                {
                    Name = ProcessServerCSApiStubsImpl.FunctionName.MARK_RESYNC_REQUIRED,
                    
                    ParameterList = new List<ParameterElement>()
                    {
                        new ParameterElement() { Name = "HostId", Value = hostId },
                        new ParameterElement() { Name = "SourceHostId", Value = m_sourceHostId },
                        new ParameterElement() { Name = "SourceVolume", Value = m_sourceDeviceName },
                        new ParameterElement() { Name = "DestHostId", Value = m_destHostId },
                        new ParameterElement() { Name = "DestVolume", Value = m_destDeviceName },
                        new ParameterElement() { Name = "HardlinkFromFile", Value = m_hardlinkFromFile },
                        new ParameterElement() { Name = "HardlinkToFile", Value = m_hardlinkToFile },
                        new ParameterElement() { Name = "resyncReasonCode", Value = m_resyncReasonCode },
                        new ParameterElement() { Name = "detectionTime", Value = m_detectionTime }
                    }
                }
            };

            Common.XmlSerializeRequest(tw, requestId, Common.BuildHeader(hostId, hmacSignature), body);
        }
    }

    internal class GetPSConfigurationRequest : ICSApiReqBuilder_TextWriter
    {
        private readonly string m_pairId;

        public GetPSConfigurationRequest(string pairid)
        {
            if (string.IsNullOrWhiteSpace(pairid))
                throw new ArgumentNullException(nameof(pairid));

            m_pairId = pairid;
        }

        public void GenerateXml(TextWriter tw, string requestId, string hostId, string hmacSignature)
        {
            var body = new BodyElement()
            {
                FunctionRequest = new FunctionRequestElement()
                {
                    Name = ProcessServerCSApiStubsImpl.FunctionName.GET_DB_DATA,

                    ParameterList = new List<ParameterElement>()
                    {
                        new ParameterElement() { Name = "HostId", Value = hostId },
                        new ParameterElement() { Name = "PairId", Value = m_pairId }
                    }
                }
            };

            Common.XmlSerializeRequest(tw, requestId, Common.BuildHeader(hostId, hmacSignature), body);
        }
    }

    internal class GetRollbackStatusRequest : ICSApiReqBuilder_TextWriter
    {
        private readonly string m_hostId;

        public GetRollbackStatusRequest(string hostid)
        {
            if (string.IsNullOrWhiteSpace(hostid))
                throw new ArgumentNullException(nameof(hostid));

            m_hostId = hostid;
        }

        public void GenerateXml(TextWriter tw, string requestId, string hostId, string hmacSignature)
        {
            var body = new BodyElement()
            {
                FunctionRequest = new FunctionRequestElement()
                {
                    Name = ProcessServerCSApiStubsImpl.FunctionName.IS_ROLLBACK_IN_PROGRESS,

                    ParameterList = new List<ParameterElement>()
                    {
                        new ParameterElement() { Name = "HostId", Value = hostId },
                        new ParameterElement() { Name = "SourceId", Value = m_hostId }
                    }
                }
            };

            Common.XmlSerializeRequest(tw, requestId, Common.BuildHeader(hostId, hmacSignature), body);
        }
    }
}
