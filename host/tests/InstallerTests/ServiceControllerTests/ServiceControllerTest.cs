using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using ASRSetupFramework;
using FluentAssertions;
using System.Diagnostics;
using System.ServiceProcess;
using System.Configuration.Install;
using System.IO;
using System.Threading;

namespace ServiceControllerTests
{
    [TestClass]
    public class ServiceControllerTests
    {
        public static string s_NonExistentService = "testservice";
        public static string s_logFolder = @"c:\test";
        public static string s_logFile = @"c:\test\servicetests.log";

        public static string s_ServicePath = @"c:\test\appservice.exe";
        public static string s_ServiceName = "appservice";
        public static bool s_isServiceInstalled = false;

        private TestContext testContextInstance;
        public TestContext TestContext
        {
            get
            {
                return testContextInstance;
            }
            set
            {
                testContextInstance = value;
            }
        }

        [ClassInitialize]
        public static void InitTestSetup(TestContext context)
        {
            if (!Directory.Exists(s_logFolder))
            {
                Directory.CreateDirectory(s_logFolder);
            }

            DeleteService(s_NonExistentService);
        }

        [ClassCleanup]
        public static void ServiceTestsCleanup()
        {
            DeleteService(s_ServiceName);
            DeleteService(s_NonExistentService);
        }

        [TestInitialize]
        public void InitServiceTestcaseSetup()
        {
            DeleteService(s_ServiceName);
        }

        [TestMethod]
        public void ServiceControllerShouldReturnFalseIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.IsInstalled(s_NonExistentService).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldReturnNullIfServiceControllerIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.IsServiceInstalled(s_NonExistentService).Should().Be(null);
        }

        [TestMethod]
        public void ServiceControllerShouldFailStartServiceIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.StartService(s_NonExistentService).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldReturnFailStopServiceIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.StopService(s_NonExistentService).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldReturnFailSetServiceDefaultFailureActionsIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.SetServiceDefaultFailureActions(s_NonExistentService).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldReturnFailSetServiceFailureActionsWith1RetryIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.SetServiceFailureActions(s_NonExistentService, 1, 30000).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldReturnFailSetServiceFailureActionsWith2RetryIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.SetServiceFailureActions(s_NonExistentService, 2, 30000).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldReturnFailSetServiceFailureActionsWith3RetryIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.SetServiceFailureActions(s_NonExistentService, 3, 30000).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldReturnFailChangeServiceStartupTypeToAutomaticIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.ChangeServiceStartupType(s_NonExistentService, NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldReturnFailChangeServiceStartupTypeToManualIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.ChangeServiceStartupType(s_NonExistentService, NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldReturnFailChangeServiceStartupTypeToDisabledIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.ChangeServiceStartupType(s_NonExistentService, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldReturn0ForGetProcessIDIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.GetProcessID(s_NonExistentService).Should().Be(0);
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedForKillProcessIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.KillServiceProcess(s_NonExistentService).Should().BeTrue();
        }

        [TestMethod]
        public void ServiceControllerShouldFailIsEnabledAndRunningIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.IsEnabledAndRunning(s_NonExistentService).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldFailIsEnabledIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.IsEnabled(s_NonExistentService).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldFailSetDelayedAutoStartIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.SetDelayedAutoStart(s_NonExistentService, true).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldFailResetDelayedAutoStartIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.SetDelayedAutoStart(s_NonExistentService, false).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldFailSetDescriptionIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.SetDescription(s_NonExistentService, "test").Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedUninstallServiceIfServiceIsNotInstalled()
        {
            ServiceControlFunctions.UninstallService(s_NonExistentService).Should().BeTrue();
        }

        [TestMethod]
        public void ServiceControllerShouldFailInstallServiceForAutoStartIfBinaryIsNotExisting()
        {
            DeleteService(s_ServiceName);
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START, "c:\\notexist\\notexist.exe").Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldFailInstallServiceForManualStartIfBinaryIsNotExisting()
        {
            DeleteService(s_ServiceName);
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START, "c:\\notexist\\notexist.exe").Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldFailInstallServiceForDisabledStartIfBinaryDoesNotExist()
        {
            DeleteService(s_ServiceName);
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED, "c:\\notexist\\notexist.exe").Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedInstallServiceForAutomaticStartIfBinaryExists()
        {
            DeleteService(s_ServiceName);
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START, s_ServicePath).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.StartType.Should().Be(ServiceStartMode.Automatic);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedInstallServiceForManualStartIfBinaryExists()
        {
            DeleteService(s_ServiceName);
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START, s_ServicePath).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.StartType.Should().Be(ServiceStartMode.Manual);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedInstallServiceForDisabledStartIfBinaryExists()
        {
            DeleteService(s_ServiceName);
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED, s_ServicePath).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.StartType.Should().Be(ServiceStartMode.Disabled);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedInstallServiceForAlreadyInstalledService()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED, s_ServicePath).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.Should().NotBe(null);
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED, s_ServicePath).Should().BeTrue();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedUnInstallServiceIfServiceExists()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.UninstallService(s_ServiceName).Should().BeTrue();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedUnInstallServiceIfServiceIsDeletedOutside()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED, s_ServicePath).Should().BeTrue();
            DeleteService(s_ServiceName);
            ServiceControlFunctions.UninstallService(s_ServiceName).Should().BeTrue();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedIsEnabledForAutomaticService()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.IsEnabled(s_ServiceName).Should().BeTrue();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedIsEnabledForSecondCallOnAutomaticService()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.IsEnabled(s_ServiceName).Should().BeTrue();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedIsEnabledForManualService()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.IsEnabled(s_ServiceName).Should().BeTrue();
        }

        [TestMethod]
        public void ServiceControllerShouldFailIsEnabledForDisabledService()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.IsEnabled(s_ServiceName).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldFailIsEnabledAndRunningForStoppedAutomaticService()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.IsEnabledAndRunning(s_ServiceName).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldFailIsEnabledAndRunningForStoppedManualService()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.IsEnabledAndRunning(s_ServiceName).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldFailIsEnabledAndRunningForStoppedDisabledService()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.IsEnabledAndRunning(s_ServiceName).Should().BeFalse();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedChangeServiceStartupTypeToAutomaticForAutomaticServices()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.ChangeServiceStartupType(s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.StartType.Should().Be(ServiceStartMode.Automatic);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedChangeServiceStartupTypeToManualForAutomaticServices()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.ChangeServiceStartupType(s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.StartType.Should().Be(ServiceStartMode.Manual);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedChangeServiceStartupTypeToDisabledForAutomaticServices()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.ChangeServiceStartupType(s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.StartType.Should().Be(ServiceStartMode.Disabled);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedChangeServiceStartupTypeToAutomaticForManualServices()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.ChangeServiceStartupType(s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.StartType.Should().Be(ServiceStartMode.Automatic);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedChangeServiceStartupTypeToManualForManualServices()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.ChangeServiceStartupType(s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.StartType.Should().Be(ServiceStartMode.Manual);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedChangeServiceStartupTypeToDisabledForManualServices()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.ChangeServiceStartupType(s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.StartType.Should().Be(ServiceStartMode.Disabled);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedChangeServiceStartupTypeToAutomaticForDisabledServices()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.ChangeServiceStartupType(s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.StartType.Should().Be(ServiceStartMode.Automatic);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedChangeServiceStartupTypeToManualForDisabledServices()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.ChangeServiceStartupType(s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.StartType.Should().Be(ServiceStartMode.Manual);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedChangeServiceStartupTypeToDisabledForDisaledServices()
        {
            ServiceControlFunctions.InstallService(s_ServiceName, s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED, s_ServicePath).Should().BeTrue();
            ServiceControlFunctions.ChangeServiceStartupType(s_ServiceName, NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED).Should().BeTrue();
            ServiceController svcController = new ServiceController(s_ServiceName);
            svcController.StartType.Should().Be(ServiceStartMode.Disabled);
            svcController.Close();
        }


        [TestMethod]
        public void ServiceControllerShouldSucceedStartServiceForStoppedService()
        {
            StopService("vss");
            ServiceControlFunctions.StartService("vss").Should().BeTrue();

            ServiceController svcController = new ServiceController("vss");
            svcController.Status.Should().Be(ServiceControllerStatus.Running);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedStartServiceForRunningService()
        {
            StartService("vss");

            ServiceControlFunctions.StartService("vss").Should().BeTrue();

            ServiceController svcController = new ServiceController("vss");
            svcController.Refresh();
            svcController.Status.Should().Be(ServiceControllerStatus.Running);
            svcController.Close();
        }

        [TestMethod]
        public void ServiceControllerShouldSucceedStopServiceForRunningService()
        {
            StartService("vss");

            ServiceControlFunctions.StopService("vss").Should().BeTrue();

            ServiceController svcController = new ServiceController("vss");
            svcController.Status.Should().Be(ServiceControllerStatus.Stopped);
            svcController.Close();
        }

        [TestMethod]

        public void ServiceControllerShouldSucceedStopServiceForStoppedService()
        {
            StopService("vss");
            ServiceControlFunctions.StopService("vss").Should().BeTrue();

            ServiceController svcController = new ServiceController("vss");
            svcController.Status.Should().Be(ServiceControllerStatus.Stopped);
            svcController.Close();
        }

        [TestMethod]

        public void ServiceControllerShouldSucceedIsEnabledAndRunningForManualServices()
        {
            ServiceControlFunctions.ChangeServiceStartupType("vss", NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START).Should().BeTrue();
            StartService("vss");

            ServiceController svcController = new ServiceController("vss");
            svcController.StartType.Should().Be(ServiceStartMode.Manual);
            ServiceControlFunctions.IsEnabledAndRunning("vss").Should().BeTrue();
            svcController.Status.Should().Be(ServiceControllerStatus.Running);
            svcController.Close();
        }

        [TestMethod]

        public void ServiceControllerShouldSucceedIsEnabledAndRunningForAutomaticServices()
        {
            ServiceControlFunctions.ChangeServiceStartupType("vss", NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START).Should().BeTrue();
            StartService("vss");

            ServiceController svcController = new ServiceController("vss");
            svcController.StartType.Should().Be(ServiceStartMode.Automatic);
            ServiceControlFunctions.IsEnabledAndRunning("vss").Should().BeTrue();
            svcController.Status.Should().Be(ServiceControllerStatus.Running);
            svcController.Close();
        }

        public void StopService(string serviceName)
        {
            ControlService(serviceName, ServiceControllerStatus.Stopped);
        }

        public void StartService(string serviceName)
        {
            ControlService(serviceName, ServiceControllerStatus.Running);
        }

        public void ControlService(string serviceName, ServiceControllerStatus status)
        {
            status.Should().Match<ServiceControllerStatus>(st => st == ServiceControllerStatus.Running || st == ServiceControllerStatus.Stopped);

            ServiceController svcController = new ServiceController(serviceName);

            if (status != svcController.Status)
            {
                if (status == ServiceControllerStatus.Stopped)
                {
                    svcController.Stop();
                }
                else
                {
                    svcController.Start();
                }
                var timeout = new TimeSpan(0, 0, 5); // 5seconds
                svcController.WaitForStatus(status, timeout);
                svcController.Status.Should().Be(status);
            }
        }

        public static void DeleteService(string serviceName)
        {
            var svcController = System.Linq.Enumerable.FirstOrDefault(ServiceController.GetServices(), s => s.ServiceName.Equals(serviceName, StringComparison.InvariantCultureIgnoreCase));
            if (null == svcController)
            {
                return;
            }
            svcController.Dispose();

            try
            {
                ServiceInstaller ServiceInstallerObj = new ServiceInstaller();
                InstallContext Context = new InstallContext(AppDomain.CurrentDomain.BaseDirectory + "\\uninstalllog.log", null);
                ServiceInstallerObj.Context = Context;
                ServiceInstallerObj.ServiceName = serviceName;
                ServiceInstallerObj.Uninstall(null);
            }
            catch (Exception)
            { }
        }
    }
}
