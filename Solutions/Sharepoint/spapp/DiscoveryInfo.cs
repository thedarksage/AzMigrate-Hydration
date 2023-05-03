using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.Xml.Serialization;

namespace spapp
{
    [XmlRootAttribute("Discovery", IsNullable = false)]
    public class DiscoveryInfo
    {
        public string AppName = "";
        public string Version = "";
        public string FarmId = "";
        public string DiscoveryServer = "";
        public string HostId = "";
        public string DomainName = "";        
        public ServerList Role = new ServerList();
        public List<WebApplication> WebApplications = new List<WebApplication>();

        public string ToXml()
        {
            StringBuilder xml = new StringBuilder();

            xml.AppendFormat("<Discovery>");
            xml.AppendFormat("<AppName>{0}", AppName);
            xml.AppendFormat("<Version>{0}</Version>", Version);
            xml.AppendFormat("<DiscoveryServer>{0}</DiscoveryServer>", DiscoveryServer);
            xml.AppendFormat("<HostId>{0}</HostId>", HostId);
            xml.AppendFormat(Role.ToXml());
            xml.AppendFormat("</AppName></Discovery>");

            return xml.ToString();
        }
    }

    public class ServerList
    {
        public List<ServerInfo> DC = new List<ServerInfo>();
        public List<ServerInfo> App = new List<ServerInfo>();
        public List<ServerInfo> Web = new List<ServerInfo>();
        public List<ServerInfo> Database = new List<ServerInfo>();
        public ServerInfo Single;

        public string ToXml()
        {
            StringBuilder xml = new StringBuilder();
            xml.AppendFormat("<Role>");
            if (DC.Count > 0)
            {
                xml.AppendFormat("<DC>");
                foreach (ServerInfo server in DC)
                {
                    xml.AppendFormat(server.ToXml());
                }
                xml.AppendFormat("</DC>");
            }
            if (App.Count > 0)
            {
                xml.AppendFormat("<App>");
                foreach (ServerInfo server in App)
                {
                    xml.AppendFormat(server.ToXml());
                }
                xml.AppendFormat("</App>");
            }
            if (Web.Count > 0)
            {
                xml.AppendFormat("<Web>");
                foreach (ServerInfo server in Web)
                {
                    xml.AppendFormat(server.ToXml());
                }
                xml.AppendFormat("</Web>");
            }
            if (Database.Count > 0)
            {
                xml.AppendFormat("<Database>");
                foreach (ServerInfo server in Database)
                {
                    xml.AppendFormat(server.ToXml());
                }
                xml.AppendFormat("</Database>");
            }
            if (Single != null)
            {
                xml.AppendFormat("<Single>");
                xml.AppendFormat(Single.ToXml());
                xml.AppendFormat("</Single>");
            }
            xml.AppendFormat("</Role>");

            return xml.ToString();
        }
    }

    //[XmlType(TypeName = "Server")]
    //[Serializable]
    public class ServerInfo
    {
        public string Name = "";
        public string IP = "";
        public string HostId = "";

        public string ToXml()
        {
            StringBuilder xml = new StringBuilder();

            xml.AppendFormat("<Server>");
            xml.AppendFormat("<Name>{0}</Name>", Name);
            xml.AppendFormat("<IP>{0}</IP>", IP);
            xml.AppendFormat("<HostId>{0}</HostId>", HostId);
            xml.AppendFormat("</Server>");

            return xml.ToString();
        }
    }

    public class WebApplication
    {
        [XmlAttribute("Name")]
        public string Name = "";

        [XmlAttribute("Port")]
        public int Port;

        public WebApplication()
        {
        }

        public WebApplication(string webAppName, int port)
        {
            Name = webAppName;
            Port = port;
        }
    }

}
