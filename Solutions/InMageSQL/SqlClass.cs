using System;
using System.Collections.Generic;
using System.Data;
using Microsoft.SqlServer.Management.Smo;

[System.Runtime.InteropServices.Guid("D4660088-308E-49fb-AB1A-77224F3FF851")]
public interface IDiscover
{
    string getSqlInstances(string HostName);
    string getDB(string SQLInstanceName);
    string getDatabaseInfo(string SQLInstanceName, string DBName);
};
namespace InMageSQL
{
    [System.Runtime.InteropServices.Guid("46A951AC-C2D9-48e0-97BE-91F3C9E7B065")]
    public class SqlClass:IDiscover
    {
        public string getSqlInstances(string HostName)
        {

            string srvname = string.Empty; string srvnames = string.Empty;
            Server srv = new Server(HostName);
            DataTable dt = SmoApplication.EnumAvailableSqlServers(false);//retrieving the network instances and compare them with HostName
            foreach (DataRow dr in dt.Rows)
            {
                srvname = (string)dr["name"];
                string server = (string)dr["Server"];   //retrieving The name of the server on which the instance of SQL Server is installed.
                
                if (server.Equals(HostName))           //checking the HostName for Machine(SQLSERVER) name
                {
                    string inst = (string)dr["name"];
                    srvnames = srvnames + inst + ";";
                }

            }
            return srvnames;

        }

        public string getDB(string instanceName)   //getting all the databases
        {
            string dbfile = string.Empty;
            try
            {
                Server srv = new Server(instanceName);
                foreach (Database db in srv.Databases)
                {
                    dbfile = dbfile + db.Name + ";";
                }
           }

            catch (Exception e)
            {
                dbfile = string.Empty;
                Console.WriteLine("[ERROR TYPE]: " + e.GetType());
                Console.WriteLine("[ERROR]: "+e.Message);
                Exception ex;
                ex = e.InnerException;
                while (!object.ReferenceEquals(ex.InnerException, null))
                {
                    Console.WriteLine(ex.InnerException.Message);
                    ex = ex.InnerException;
                }
            }
            return dbfile;
        }
        public string getDatabaseInfo(string instanceName, string DB)
        {
           string searchnamedinstance = instanceName;
               string dbname = string.Empty, DatabaseInfo = string.Empty;
               try
               {
                   Server srv = new Server(instanceName);
                   foreach (Database db in srv.Databases)
                   {
                       if (DB.Equals(db.Name))
                       {
                           foreach (FileGroup filegroup in db.FileGroups)
                           {
                               foreach (DataFile datafile in filegroup.Files)
                               {
                                   DatabaseInfo += datafile.FileName + ";";
                               }
                           }
                           foreach (LogFile logfile in db.LogFiles)
                           {
                               DatabaseInfo += logfile.FileName + ";";
                           }
                       }
                   }
                   
               }

            
               catch (Exception e)
               {
                   DatabaseInfo = string.Empty;
                   Console.WriteLine("[ERROR TYPE]:" + e.GetType());
                   Console.WriteLine("[ERROR]: "+e.Message);
                   Exception ex;
                   ex = e.InnerException;
                   while (!object.ReferenceEquals(ex.InnerException, null))
                   {
                       Console.WriteLine(ex.InnerException.Message);
                       ex = ex.InnerException;
                   }
               }

            return DatabaseInfo;

        }

    }
}



