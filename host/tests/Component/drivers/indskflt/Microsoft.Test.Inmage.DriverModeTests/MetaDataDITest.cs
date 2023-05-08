using System;
using InMageLogger;
using FluentAssertions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Microsoft.Test.Inmage.DriverModeTests
{
    [TestClass]
    public class MetaDataDITest : BaseDITests
    {
        [ClassInitialize]
        public static void InitTestSetup(TestContext context)
        {
            DriverModeTestsClassInitialize(context, DriverMode.MetaData, 1, @"C:\tmp\MetaDataDITest.log");

            InitializeReplication(context.TestName + "_" + context.FullyQualifiedTestClassName, 0, 1024 * 1024);

            s_logger.LogInfo("Set Metadata mode");
            StartSVAgents(s_s2Agent);
        }

        [ClassCleanup]
        public static void ClassCleanup()
        {
            DriverModeTestsClassCleanUp();
        }

    }
}
