using System;
using System.IO;

namespace MarsAgent.CBEngine.Utilities
{
    public class FileLock : IDisposable
    {
        private FileStream _lock;
        public FileLock(string path)
        {
            if (File.Exists(path))
            {
                _lock = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
                IsLocked = true;
            }
        }

        public bool IsLocked { get; set; }

        public void Dispose()
        {
            if (_lock != null)
            {
                IsLocked = false;
                _lock.Dispose();
            }
        }
    }
    
}
