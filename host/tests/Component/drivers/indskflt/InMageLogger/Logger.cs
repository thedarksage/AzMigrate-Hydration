using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace InMageLogger
{
    public class Logger
    {
        protected readonly object lockObj = new object();

        private string logName;
        public  string LogName
        {
            get { return logName; }
            set { logName = value; }
        }

        private StreamWriter sw;
        public StreamWriter Writer
        {
            get { return sw; }
            set { sw = value; }
        }


        public Logger(string logFile, bool append)
        {
            if (String.IsNullOrWhiteSpace(logFile))
            {
                throw new ArgumentException("Log file was not set");
            }
            this.logName = logFile;
            this.sw = new StreamWriter(this.logName, append);
        }

        public void LogInfo(string message)
        {
            lock (lockObj)
            {
                sw.WriteLine(DateTime.Now.ToString() + " : INFO : " + message);
                sw.Flush();
            }
        }

        public void LogInfo(string format, params object[] args)
        {
            lock (lockObj)
            {
                sw.WriteLine(DateTime.Now.ToString() + " : INFO : " + format, args);
                sw.Flush();
            }
        }

        public void LogWarning(string message)
        {
            lock (lockObj)
            {
                sw.WriteLine(DateTime.Now.ToString() + " : WARNING : " + message);
                sw.Flush();
            }
        }

        public void LogError(string message, Exception ex)
        {
            lock (lockObj)
            {
                sw.WriteLine(DateTime.Now.ToString() + " : ERROR : " + message + " with error " + ex.Message);
                sw.Flush();
            }
        }

        public void WriteLine(string message)
        {
            sw.WriteLine(DateTime.Now.ToString() + " : " + message);
        }

        public void Write(string message)
        {
            sw.WriteLine(DateTime.Now.ToString() + " : " + message);
        }

        public void Close()
        {
            if (this.sw != null)
            {
                sw.Flush();
                sw.Close();
                sw = null;
            }
        }
    }
}
