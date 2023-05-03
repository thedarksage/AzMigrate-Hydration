using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net;

namespace InmCheckAzureServices
{
    class Program
    {
        static void Usage()
        {
            string appname = System.AppDomain.CurrentDomain.FriendlyName;
            Console.WriteLine("Usage : {0} <uri> <timeout> [proxy]", appname);
        }

        static int Main(string[] args)
        {
            if ((args.Length != 2) && (args.Length != 3))
            {
                Usage();
                return 1;
            }

            try
            {
                Uri uri = new Uri(args[0]);
                Int32 timeout = Convert.ToInt32(args[1]);

                WebRequest request = WebRequest.Create(uri);
                request.Timeout = timeout * 1000;

                if (args.Length == 3)
                {
                    request.Proxy = new WebProxy(args[2]);
                }

                request.GetResponse();
            }
            catch (WebException ex)
            {
                if (ex.Status == WebExceptionStatus.ProtocolError)
                {
                    HttpWebResponse response = ex.Response as HttpWebResponse;

                    Console.WriteLine("Failed to access the URI {0} with error {1}",
                                args[0],
                                ex);
                }
                else if (ex.Status == WebExceptionStatus.TrustFailure)
                {
                    Console.WriteLine("Failed to access the URI {0} with trust failure error {1}",
                               args[0],
                               ex);
                    return 1;
                }
                else
                {
                    Console.WriteLine("Failed to access the URI {0} with error {1}",
                                args[0],
                                ex);
                }
                return 0;
            }
            catch (Exception ex)
            {
                Console.WriteLine("Faile to access the URI {0} with exception {1}",
                    args[0],
                    ex);
                return 1;
            }

            return 0;
        }
    }
}
