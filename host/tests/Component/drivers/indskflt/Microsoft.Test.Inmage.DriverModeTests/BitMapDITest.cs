using System;
using InMageLogger;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Microsoft.Test.Inmage.DriverModeTests
{
    [TestClass]
    public class BitMapDITest : BaseDITests
    {
        [ClassInitialize]
        public static void InitTestSetup(TestContext context)
        {
            DriverModeTestsClassInitialize(context, DriverMode.BitMap, 1, @"C:\tmp\BitMapDITest.log");

            InitializeReplication(context.TestName + "_" + context.FullyQualifiedTestClassName, 0, 1024 * 1024);
        }

        [ClassCleanup]
        public static void ClassCleanup()
        {
            DriverModeTestsClassCleanUp();
        }
    }
}
