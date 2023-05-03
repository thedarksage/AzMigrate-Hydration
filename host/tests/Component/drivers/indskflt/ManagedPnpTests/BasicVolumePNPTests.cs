using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace ManagedPnpTests
{
    [TestClass]
    public abstract class BasicVolumePNPTests : VolumePNPTests
    {
        public static void BasicVolumePnpClassInit(int numDisks, string logdir)
        {
            PnPTestsInitialize(numDisks, logdir);
        }
    }
}
