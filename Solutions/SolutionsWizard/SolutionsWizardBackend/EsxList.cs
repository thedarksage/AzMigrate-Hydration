using System;
using System.Collections.Generic;
using System.Text;
using System.Collections;
using System.Diagnostics;
using System.IO;
using System.Runtime.Serialization.Formatters.Binary;


namespace Com.Inmage.Esxcalls
{
    [Serializable]
    public class EsxList
    {
        public ArrayList _esxList;

        public EsxList()
        {
            _esxList = new ArrayList();
        }


        public bool AddOrReplaceHost(Esx inEsx)
        {
            try
            {
                int index = -1;

                Esx h = new Esx();

                h = (Esx)inEsx.Clone();

                if (DoesHostExist(inEsx, ref index) == true)
                {
                    _esxList.RemoveAt(index);
                    _esxList.Add(h);


                }
                else
                {
                    _esxList.Add(h);
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }
            return true;
        }

        public bool DoesHostExist(Esx inEsx, ref int inIndex)
        {
            try
            {
                // Debug.WriteLine("Entered: DoesHostExist");
                int index = 0;

                foreach (Esx h in _esxList)
                {
                    //   Debug.WriteLine( "\tComparing " +  h.displayName + " With " + inEsx.displayName);


                    if (inEsx.ip != null && inEsx.ip.Length > 0)
                    {
                        if (h.ip != null && h.ip.Length > 0)
                        {
                            if (h.ip.CompareTo(inEsx.ip) == 0)
                            {
                                inIndex = index;

                                Console.WriteLine("Matched IPs " + inEsx.ip + "and " + h.ip + "Index = " + index);

                                return true;
                            }
                        }
                    }

                    index++;

                }
                //  Debug.WriteLine("Exiting: DoesHostExist");
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }
            return false;
        }

        public object Clone()
        {

            MemoryStream memoryStream = new MemoryStream();

            BinaryFormatter binaryformatter = new BinaryFormatter();

            binaryformatter.Serialize(memoryStream, this);

            memoryStream.Position = 0;

            object obj = binaryformatter.Deserialize(memoryStream);

            memoryStream.Close();

            return obj;

        }


        internal bool Print()
        {
            try
            {
                foreach (Esx h in _esxList)
                {
                    h.Print();
                }

            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }

            return true;
        }

    }
}
