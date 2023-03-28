using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Configuration;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GeneratePropertySheet
{
    class Program
    {
        static void Main(string[] args)
        {
            string fileVersion = string.Empty;
            string prodVersion = string.Empty;            

            Console.WriteLine(string.Format("Parsing {0} for version information", args[1]));

            // Parse the version file and get line with AssemblyVersion in it.
            foreach (string line in File.ReadLines(args[0]))
            {
                if (line.Contains("AssemblyFileVersion"))
                {
                    string[] lineParts = line.Split('"');
                    Console.WriteLine("File Version is " + lineParts[1]);
                    fileVersion = lineParts[1];
                }
				
				if (line.Contains("AssemblyInformationalVersion"))
                {
                    string[] lineParts = line.Split('"');
                    Console.WriteLine("Product Version is " + lineParts[1]);
                    prodVersion = lineParts[1];
                }
            }

            // Dump the whole of settings in app.config into the PropertySheet
            List<string> propertySheet = new List<string>();
            NameValueCollection nameValCol = ConfigurationManager.AppSettings;

            foreach(string key in nameValCol.AllKeys)
            {
                propertySheet.Add(string.Format("{0}={1}", key, nameValCol[key]));
            }

            propertySheet.Add(string.Format("{0}={1}", "FileVersion", fileVersion));
            propertySheet.Add(string.Format("{0}={1}", "ProductVersion", prodVersion));

            // Write PropertySheet to file
            File.WriteAllLines(args[1], propertySheet.ToArray());
        }
    }
}
