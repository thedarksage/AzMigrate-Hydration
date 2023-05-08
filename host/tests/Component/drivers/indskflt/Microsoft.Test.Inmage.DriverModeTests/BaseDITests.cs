using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Threading;
using ReplicationEngine;
using Microsoft.Test.Storages;
using FluentAssertions;
using System.IO;
using InMageLogger;
using Microsoft.Test.Inmage.DriverMode;

namespace Microsoft.Test.Inmage.DriverModeTests
{
    [TestClass]
    public abstract class BaseDITests : DriverModes
    {
        [TestMethod]
        public void DIFullDiskTestofSectorSizedWrites()
        {
            StartAndValidateWrites(1);
        }

        [TestMethod]
        public void DIFullDiskTestof1KSizedWrites()
        {
            StartAndValidateWrites(2);
        }


        [TestMethod]
        public void DIFullDiskTestof1KSizedUnalignedWrites()
        {
            StartAndValidateWrites(2, true);
        }

        [TestMethod]
        public void DIFullDiskTestof4KSizedWrites()
        {
            StartAndValidateWrites(8);
        }

        [TestMethod]
        public void DIFullDiskTestof4KSizedUnalignedWrites()
        {
            StartAndValidateWrites(8, true);
        }

        [TestMethod]
        public void DIFullDiskTestof8KSizedWrites()
        {
            StartAndValidateWrites(16);
        }

        [TestMethod]
        public void DIFullDiskTestof8KSizedUnalignedWrites()
        {
            StartAndValidateWrites(16, true);
        }

        [TestMethod]
        public void DIFullDiskTestof16KSizedWrites()
        {
            StartAndValidateWrites(32);
        }

        [TestMethod]
        public void DIFullDiskTestof16KSizedUnalignedWrites()
        {
            StartAndValidateWrites(32, true);
        }

        [TestMethod]
        public void DIFullDiskTestof32KSizedWrites()
        {
            StartAndValidateWrites(64);
        }

        [TestMethod]
        public void DIFullDiskTestof32KSizedUnalignedWrites()
        {
            StartAndValidateWrites(64, true);
        }

        [TestMethod]
        public void DIFullDiskTestof64KSizedWrites()
        {
            StartAndValidateWrites(128);
        }

        [TestMethod]
        public void DIFullDiskTestof64KSizedUnalignedWrites()
        {
            StartAndValidateWrites(128, true);
        }


        [TestMethod]
        public void DIFullDiskTestof128KSizedWrites()
        {
            StartAndValidateWrites(256);
        }

        [TestMethod]
        public void DIFullDiskTestof128KSizedUnalignedWrites()
        {
            StartAndValidateWrites(256, true);
        }

        [TestMethod]
        public void DIFullDiskTestof256KSizedWrites()
        {
            StartAndValidateWrites(512);
        }

        [TestMethod]
        public void DIFullDiskTestof256KSizedUnalignedWrites()
        {
            StartAndValidateWrites(512, true);
        }

        [TestMethod]
        public void DIFullDiskTestof18KSizedWrites()
        {
            StartAndValidateWrites(18432, true);
        }

        [TestMethod]
        public void DIFullDiskTestof1MSizedWrites()
        {
            StartAndValidateWrites(2 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof1MSizedUnalignedWrites()
        {
            StartAndValidateWrites(2 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof2MSizedWrites()
        {
            StartAndValidateWrites(4096, true);
        }

        [TestMethod]
        public void DIFullDiskTestof2MSizedUnalignedWrites()
        {
            StartAndValidateWrites(4096, true);
        }

        [TestMethod]
        public void DIFullDiskTestof3MSizedWrites()
        {
            StartAndValidateWrites(6 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof3MSizedUnalignedWrites()
        {
            StartAndValidateWrites(6 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof4MSizedWrites()
        {
            StartAndValidateWrites(8 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof4MSizedUnalignedWrites()
        {
            StartAndValidateWrites(8 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof5MSizedWrites()
        {
            StartAndValidateWrites(10 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof5MSizedUnalignedWrites()
        {
            StartAndValidateWrites(10 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof6MSizedWrites()
        {
            StartAndValidateWrites(12 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof6MSizedUnalignedWrites()
        {
            StartAndValidateWrites(12 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof7MSizedWrites()
        {
            StartAndValidateWrites(14 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof7MSizedUnalignedWrites()
        {
            StartAndValidateWrites(14 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof8MSizedWrites()
        {
            StartAndValidateWrites(16384, true);
        }

        [TestMethod]
        public void DIFullDiskTestof8MSizedUnalignedWrites()
        {
            StartAndValidateWrites(16384, true);
        }

        [TestMethod]
        public void DIFullDiskTestof9MSizedWrites()
        {
            StartAndValidateWrites(18 * 1024, true);
        }

        [TestMethod]
        public void DIFullDiskTestof9MSizedUnalignedWrites()
        {
            StartAndValidateWrites(18 * 1024, true);
        }
    }
}

