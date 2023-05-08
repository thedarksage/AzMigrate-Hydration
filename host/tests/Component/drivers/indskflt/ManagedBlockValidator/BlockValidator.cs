using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Test.Storages;
using System.Security.Cryptography;
using System.IO;
using InMageLogger;

namespace ManagedBlockValidator
{
    public class BlockValidator
    {
        public static bool Validate(IReadableStorage srcStorage, IReadableStorage targetStorage, ulong ulSize, Logger logger, ulong startOffset = 0)
        {
            byte[] srcBuffer = new byte[1024 * 1024];
            byte[] targetBuffer = new byte[1024 * 1024];

            ulong offset = (startOffset != 0) ? startOffset : 0;

            ulong ulBytesToRead = 1024*1024;
            MD5 Md5Hash = MD5.Create();

            while (offset < ulSize)
            {
                ulBytesToRead = ((ulSize - offset) < ulBytesToRead) ? (ulSize - offset) : ulBytesToRead;
               
                if (ulBytesToRead != srcStorage.Read(srcBuffer, (uint) ulBytesToRead, offset))
                {
                    return false;
                }

                if (ulBytesToRead != targetStorage.Read(targetBuffer, (uint)ulBytesToRead, offset))
                {
                    return false;
                }

                byte[] srcMd5Hash = Md5Hash.ComputeHash(srcBuffer);
                byte[] targetMd5Hash = Md5Hash.ComputeHash(targetBuffer);

                if (!srcMd5Hash.SequenceEqual(targetMd5Hash))
                {
                    logger.LogInfo("DI Validation failed at offset : " + offset);
                    logger.LogInfo("Source Buffer  : " + string.Join(",", srcBuffer));
                    logger.LogInfo("Target Buffer  : " + string.Join(",", targetBuffer));
                    return false;
                }

                offset += ulBytesToRead;
            }

            return true;
        }
    }
}
