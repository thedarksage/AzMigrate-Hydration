using System;
using System.IO;
using LoggerInterface;
using MarsAgent.LoggerInterface;

namespace MarsAgent.CBEngine.Utilities
{
    public static class DirectoryUtil
    {
        public static string GetLogDirectory(string rootdirpath, string dirprefix)
        {
            if (!string.IsNullOrEmpty(rootdirpath))
            {
                string datetime = DateTime.Now.ToString("MM-dd-yyyy-HH");

                string dirname = string.Concat(dirprefix, "-", datetime);

                string dirpath = Path.Combine(rootdirpath, dirname);

                //Creates all directories and subdirectories in the specified path unless they already exist.
                if (!Directory.Exists(dirpath))
                {
                    Directory.CreateDirectory(dirpath);
                }

                return dirpath;
            }
            else
            {
                Logger.Instance.LogError(
                    CallInfo.Site(),
                    $"Invalid log directory creation called: '{rootdirpath}'.\n");
                return null;
            }
        }
    }
}
