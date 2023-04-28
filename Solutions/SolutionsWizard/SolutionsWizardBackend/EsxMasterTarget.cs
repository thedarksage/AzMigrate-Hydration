using System;
using System.Collections.Generic;
using System.Text;
using System.Collections;
using com.InMage.Tools;
using System.Diagnostics;

namespace com.InMage.ESX
{

    class EsxMasterTarget
    {


        public RemoteWin remote;
        public bool _connected = false;


        public EsxMasterTarget(string inIp, string inDomain, string inUserName, string inPassWord)
        {
            remote = new RemoteWin(inIp, inDomain, inUserName, inPassWord);

            if (remote.Connect() == false)
            {
                _connected = false;
            }
            else
            {
                _connected = true;
            }
        }



        public bool IsWmiEnabled()
        {
            return _connected;
        }


        public ArrayList getListOfProtectedVms()
        {
            int processId;
            StringBuilder output;

            Debug.WriteLine("Entered: getListOfProtectedVms");
            ArrayList protectedHostList = new ArrayList();


            //Step1: Check if the WMI connection is already present or not.
            if (_connected == false)
            {
                return null;
            }
            
            // processId = remote.Execute("net start \"InMage DR-Scout FX Agent\" ");
            Console.WriteLine("Executing remote command ");


            //Step2: Execute a command 
            output = remote.ExecuteWithOutput("cmd /c dir > " );
                       

            Debug.WriteLine("output= " + output);
                      
            return null;
            
        }



        public bool RollBack( string hostName )
        {
            return true;
        }



        public bool Recover( string hostName)
        {
             return true;
        }



        public bool AddDrivers(string hostName)
        {
             return true;
        }

    }
}
