using System;
using InMageLogger;
using FluentAssertions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Microsoft.Test.Inmage.DriverModeTests
{
    [TestClass]
    public class DataModeDITest : BaseDITests 
    {
        [ClassInitialize]
        public static void DataInitTestSetup(TestContext context)
        {
            DriverModeTestsClassInitialize(context, DriverMode.Data, 1, @"C:\tmp\DataModeDITest.log");
            
            InitializeReplication(context.TestName+"_"+context.FullyQualifiedTestClassName, 0, 1024 * 1024);

            s_logger.LogInfo("Set Data Mode");
            StartSVAgents(s_s2Agent, SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING); 
        }

        [ClassCleanup]
        public static void ClassCleanup()
        {
            DriverModeTestsClassCleanUp();
        }
    }
}
