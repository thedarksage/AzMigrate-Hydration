using Microsoft.Test.Utils.NativeMacros;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Microsoft.Test.IOCTLs
{
    public class InOutMap
    {
        public Type inType;
        public Type outType;
        public InOutMap(Type inT, Type outT)
        {
            inType = inT;
            outType = outT;
        }
    };

    public class IOCTLList<T>
    {
        public Dictionary<UInt32, InOutMap> IOCTLStructDict = new Dictionary<UInt32, InOutMap>()
            {
               {IoControlCodes.IOCTL_INMAGE_GET_VERSION, new InOutMap(null, typeof(IOCTLStructures.DRIVER_VERSION)) },
               {IoControlCodes.IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY, new InOutMap(typeof(IOCTLStructures.SHUTDOWN_NOTIFY_INPUT), null) },
               {IoControlCodes.IOCTL_INMAGE_PROCESS_START_NOTIFY, new InOutMap(typeof(IOCTLStructures.PROCESS_START_NOTIFY_INPUT), null) },
               {IoControlCodes.IOCTL_INMAGE_CLEAR_DIFFERENTIALS, new InOutMap(typeof(IOCTLStructures.CLEAR_DIFFERENTIALS_DATA), null) },
               {IoControlCodes.IOCTL_INMAGE_START_FILTERING_DEVICE, new InOutMap(typeof(IOCTLStructures.START_FILTERING_DATA), null) },
               {IoControlCodes.IOCTL_INMAGE_STOP_FILTERING_DEVICE, new InOutMap(typeof(IOCTLStructures.STOP_FILTERING_DATA), null) },
               {IoControlCodes.IOCTL_INMAGE_RESYNC_START_NOTIFICATION, new InOutMap(typeof(IOCTLStructures.RESYNC_START_INPUT), typeof(IOCTLStructures.RESYNC_START_OUTPUT)) },
               {IoControlCodes.IOCTL_INMAGE_RESYNC_END_NOTIFICATION, new InOutMap(typeof(IOCTLStructures.RESYNC_END_INPUT), typeof(IOCTLStructures.RESYNC_END_OUTPUT)) },
               {IoControlCodes.IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS, new InOutMap(typeof(IOCTLStructures.COMMIT_TRANSACTION), typeof(IOCTLStructures.UDIRTY_BLOCK_V2)) },
               {IoControlCodes.IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS, new InOutMap(typeof(IOCTLStructures.COMMIT_TRANSACTION), typeof(IOCTLStructures.UDIRTY_BLOCK_V2)) },
               {IoControlCodes.IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT, new InOutMap(typeof(IOCTLStructures.VOLUME_DB_EVENT_INFO), null) }
                
            };
    }
}
