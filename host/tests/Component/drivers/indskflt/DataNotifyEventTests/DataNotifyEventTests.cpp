#include "stdafx.h"
#include "CppUnitTest.h"

#include "IPlatformAPIs.h"
#include "PlatformAPIs.h"

#include "IBlockDevice.h"
#include "CDiskDevice.h"

#include "Logger.h"

#include "InmageIoctlController.h"

#include <vector>
#include <string>

#include "boost/uuid/uuid.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "boost/uuid/uuid_generators.hpp"
#include "boost/lexical_cast.hpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace DataNotifyEventTests
{
    TEST_CLASS(DataNotifyEventTests)
    {
    private:
        static CInmageIoctlController*    s_pIoctlController;
        static IPlatformAPIs*             s_platformApis;
        static CLogger*                   s_pLogger;
        static IBlockDevice*              s_pSrcDisk;
        static UDIRTY_BLOCK_V2              s_uDirtyBlockV2;
        static COMMIT_TRANSACTION          s_commitTrans;
        static std::vector<char>          s_buffer;
        static Event                      s_notifyEvent;
    public:
        TEST_CLASS_INITIALIZE(ClassInitialize)
        {
            s_platformApis = new CWin32APIs();
            Assert::IsNotNull(s_platformApis);

            s_pLogger = new CLogger("c:\\tmp\\DataNotifyEventTests.log");
            Assert::IsNotNull(s_pLogger);

            s_pIoctlController = new CInmageIoctlController(s_platformApis, s_pLogger);
            Assert::IsNotNull(s_pIoctlController);

            s_pSrcDisk = new CDiskDevice(5, s_platformApis);
            Assert::IsNotNull(s_pSrcDisk);

            s_pIoctlController->ServiceShutdownNotify(SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING);
            s_pIoctlController->ProcessStartNotify(0);

            s_pIoctlController->StopFiltering(std::string(), true, true);
            s_pIoctlController->StartFiltering(s_pSrcDisk->GetDeviceId());
            s_pIoctlController->ClearDifferentials(s_pSrcDisk->GetDeviceId());

            s_pIoctlController->RegisterForSetDirtyBlockNotify(s_pSrcDisk->GetDeviceId(), s_notifyEvent.Self());

            // Remove First TSO
            std::string deviceId = s_pSrcDisk->GetDeviceId();
            wstring devId(deviceId.begin(), deviceId.end());

            ZeroMemory(&s_commitTrans, sizeof(s_commitTrans));
            StringCchPrintf(s_commitTrans.DevID, ARRAYSIZE(s_commitTrans.DevID), L"%s", devId.c_str());

            s_pIoctlController->GetDirtyBlockTransaction(&s_commitTrans, &s_uDirtyBlockV2, true, s_pSrcDisk->GetDeviceId());
            Assert::IsTrue(TEST_FLAG(s_uDirtyBlockV2.uHdr.Hdr.ulFlags, UDIRTY_BLOCK_FLAG_TSO_FILE));

            s_commitTrans.ulTransactionID = s_uDirtyBlockV2.uHdr.Hdr.uliTransactionID;
            s_pIoctlController->CommitTransaction(&s_commitTrans);

            int i = 0;
            // Move to data mode
            for (int i = 0; i<20; i++) {
                if (InsertLocalCrashTag()) {
                    CommitCurrentTransaction();
                    break;
                }
                CommitCurrentTransaction();
                Sleep(100);
            }
            Assert::IsTrue(i < 20);
        }

        TEST_CLASS_CLEANUP(ClassCleanup)
        {
            if (NULL != s_pSrcDisk)
            {
                s_pIoctlController->StopFiltering(s_pSrcDisk->GetDeviceId(), true, false);
                delete s_pSrcDisk;
            }
            SafeDelete(s_pIoctlController);
            SafeDelete(s_pLogger);
            SafeDelete(s_platformApis);
        }

        TEST_METHOD(NotifyEventShouldNotBeSetWhenNoDataIsWritten)
        {
            DWORD dwBytesWritten;
            Assert::IsTrue(WAIT_TIMEOUT == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            CommitCurrentTransaction(1);
        }

        TEST_METHOD(NotifyEventShouldNotBeSetWhenTillLessThan4MBDataIsWritten)
        {
            DWORD dwBytesWritten;
            for (int i = 0; i<3; i++) {
                Assert::IsTrue(s_pSrcDisk->SeekFile(BEGIN, 1024));
                Assert::IsTrue(s_pSrcDisk->Write(&s_buffer[0], 1024 * 1024, dwBytesWritten));
                Assert::IsTrue(WAIT_TIMEOUT == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            }
            CommitCurrentTransaction(1);
        }

        TEST_METHOD(NotifyEventShouldBeSetWhen4MBDataIsWrittenIn1MBBlocks)
        {
            DWORD dwBytesWritten;
            for (int i = 0; i < 3; i++) {
                Assert::IsTrue(s_pSrcDisk->SeekFile(BEGIN, 1024));
                Assert::IsTrue(s_pSrcDisk->Write(&s_buffer[0], 1024 * 1024, dwBytesWritten));
                DWORD Status = WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10);
                Assert::IsTrue(WAIT_TIMEOUT == Status);
            }

            Assert::IsTrue(s_pSrcDisk->SeekFile(BEGIN, 1024));
            Assert::IsTrue(s_pSrcDisk->Write(&s_buffer[0], 1024 * 1024, dwBytesWritten));
            DWORD Status = WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10);
            Assert::IsTrue(WAIT_OBJECT_0 == Status);
            CommitCurrentTransaction(1);
            // Clear any pending data
            CommitCurrentTransaction(1);
        }

        TEST_METHOD(NotifyEventShouldBeSetWhen4MBDataIsWrittenInSingleBlock)
        {
            DWORD dwBytesWritten;

            Assert::IsTrue(s_pSrcDisk->SeekFile(BEGIN, 1024));
            Assert::IsTrue(s_pSrcDisk->Write(&s_buffer[0], 4 * 1024 * 1024, dwBytesWritten));
            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            // 4MB is divided into 2 blocks
            CommitCurrentTransaction(2);
        }

        TEST_METHOD(NotifyEventShouldBeSetWhenNewLocalCrashTagIsInserted)
        {
            InsertLocalCrashTag();
            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            // Only tag should be drained
            CommitCurrentTransaction(1);
        }

        TEST_METHOD(NotifyEventShouldBeSetWhenNewDistCrashTagIsCommitted)
        {
            InsertDistCrashTag(true);
            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            // Only distributed tag should be drained
            CommitCurrentTransaction(1);
        }

        TEST_METHOD(NotifyEventShouldNotBeSetWhenNewDistCrashTagIsReverted)
        {
            InsertDistCrashTag(false);
            // For reverted tags no event should be set
            Assert::IsTrue(WAIT_TIMEOUT == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            CommitCurrentTransaction(1);
        }

        TEST_METHOD(NotificationShouldBeSetWhenMultipleLocalCrashTagDBIsInQueue)
        {
            ULONG ulNumTags = 10;
            for (ULONG l = 0; l < ulNumTags; l++) {
                InsertLocalCrashTag();
            }

            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            CommitCurrentTransaction(ulNumTags);
        }

        TEST_METHOD(NotificationShouldBeSetWhenMultipleDistCrashTagDBIsInQueue)
        {
            CommitCurrentTransaction();
            ULONG ulNumTags = 10;
            for (ULONG l = 0; l < ulNumTags; l++) {
                InsertDistCrashTag(true);
            }

            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            CommitCurrentTransaction(ulNumTags);
        }

        TEST_METHOD(NotificationShouldBeSetWhenFewDistCrashTagsAreCommittedWhileOtherRevokedAlternate)
        {
            ULONG ulNumTags = 10;
            for (ULONG l = 0; l < ulNumTags; l++) {
                // Revoke Every Other Tags
                InsertDistCrashTag((0 == l % 2));
            }

            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            CommitCurrentTransaction(ulNumTags / 2);
        }

        TEST_METHOD(NotificationShouldBeSetWhenFewDistCrashTagsAreCommittedWhileOtherRevokedRandomly)
        {
            InsertDistCrashTag(false);
            InsertDistCrashTag(true); // 1
            InsertDistCrashTag(false);
            InsertDistCrashTag(true); //2
            InsertDistCrashTag(true); //3
            InsertDistCrashTag(true); //4
            InsertDistCrashTag(false);
            InsertDistCrashTag(false);
            InsertDistCrashTag(false);

            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            CommitCurrentTransaction(4);
        }

        TEST_METHOD(NotificationShouldBeSetWhenLocalCrashTagsAndDataAreInQueue)
        {
            DWORD dwBytesWritten;
            ULONG ulNumTags = 10;
            for (ULONG l = 0; l < ulNumTags; l++) {
                Assert::IsTrue(s_pSrcDisk->SeekFile(BEGIN, 1024));
                Assert::IsTrue(s_pSrcDisk->Write(&s_buffer[0], 1024 * 1024, dwBytesWritten));
                InsertLocalCrashTag();
            }

            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            CommitCurrentTransaction(ulNumTags * 2);
        }

        TEST_METHOD(NotificationShouldBeSetWhenDistCrashTagsAndDataAreInQueue)
        {
            CommitCurrentTransaction();
            DWORD dwBytesWritten;
            ULONG ulNumTags = 10;
            for (ULONG l = 0; l < ulNumTags; l++) {
                Assert::IsTrue(s_pSrcDisk->SeekFile(BEGIN, 1024));
                Assert::IsTrue(s_pSrcDisk->Write(&s_buffer[0], 1024 * 1024, dwBytesWritten));
                InsertDistCrashTag(true);
            }

            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            CommitCurrentTransaction(ulNumTags * 2);
        }

        TEST_METHOD(NotificationShouldBeSetWhenRevokeAndCommitDistCrashTagsAndDataAreInQueue)
        {
            DWORD dwBytesWritten;
            ULONG ulNumTags = 10;
            for (ULONG l = 0; l < ulNumTags; l++) {
                Assert::IsTrue(s_pSrcDisk->SeekFile(BEGIN, 1024));
                Assert::IsTrue(s_pSrcDisk->Write(&s_buffer[0], 1024 * 1024, dwBytesWritten));
                InsertDistCrashTag((0 == l % 2));
            }

            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            CommitCurrentTransaction(ulNumTags + (ulNumTags / 2));
        }

        TEST_METHOD(NotifyEventShouldBeSetWhenDistTagIsInsertedWithPreviousDirtyBlockIsOpen)
        {
            DWORD dwBytesWritten;
            Assert::IsTrue(s_pSrcDisk->SeekFile(BEGIN, 1024));
            Assert::IsTrue(s_pSrcDisk->Write(&s_buffer[0], 1024 * 1024, dwBytesWritten));

            // It is a 1MB block. So no event
            Assert::IsTrue(WAIT_TIMEOUT == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));

            InsertDistCrashTag(true);

            // Tag should close earlier dirty block
            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));

            // Total drained dirty block should be 2.
            CommitCurrentTransaction(2);
        }

        TEST_METHOD(NotifyEventShouldBeSetForDistTagWithRevokeIsInsertedWithPreviousDirtyBlockIsOpen)
        {
            DWORD dwBytesWritten;
            Assert::IsTrue(s_pSrcDisk->SeekFile(BEGIN, 1024));
            Assert::IsTrue(s_pSrcDisk->Write(&s_buffer[0], 1024 * 1024, dwBytesWritten));
            Assert::IsTrue(WAIT_TIMEOUT == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));

            InsertDistCrashTag(false);

            // Even for reverted tag older data should still be drained
            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));

            // Only one dirty block should be drained
            CommitCurrentTransaction(1);

        }

        TEST_METHOD(NotifyEventShouldBeSetWhenLocalCrashTagIsInsertedWithPreviousDirtyBlockIsOpen)
        {
            DWORD dwBytesWritten;

            Assert::IsTrue(s_pSrcDisk->SeekFile(BEGIN, 1024));
            Assert::IsTrue(s_pSrcDisk->Write(&s_buffer[0], 1024 * 1024, dwBytesWritten));
            Assert::IsTrue(WAIT_TIMEOUT == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));

            InsertLocalCrashTag();

            Assert::IsTrue(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
            // 2 DB
            // 1 for premature closure of earleir dirty block
            // 1 for local crash tag
            CommitCurrentTransaction(2);
        }

        static bool InsertLocalCrashTag()
        {
            DWORD dwBytesWritten;
            boost::uuids::uuid uuid = boost::uuids::random_generator()();
            std::string tag = boost::lexical_cast<std::string>(uuid);

            uuid = boost::uuids::random_generator()();
            std::string tagContext = boost::lexical_cast<std::string>(uuid);

            std::list<std::string> taglist;
            taglist.push_back(tag);

            return s_pIoctlController->LocalCrashConsistentTagIoctl(tag, taglist, (SV_ULONG)TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS, 100);
        }

        static void InsertDistCrashTag(bool bCommit)
        {
            DWORD dwBytesWritten;
            ULONG	ulFlags = TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS;
            if (bCommit) {
                ulFlags |= TAG_INFO_FLAGS_COMMIT_TAG;
            }
            boost::uuids::uuid uuid = boost::uuids::random_generator()();
            std::string tag = boost::lexical_cast<std::string>(uuid);

            uuid = boost::uuids::random_generator()();
            std::string tagContext = boost::lexical_cast<std::string>(uuid);

            std::list<std::string> taglist;
            taglist.push_back(tag);

            s_pIoctlController->HoldWrites(tagContext, TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS, 1000);
            s_pIoctlController->InsertDistTag(tagContext, TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS, taglist);
            s_pIoctlController->ReleaseWrites(tagContext, TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS);
            s_pIoctlController->CommitTag(tagContext, ulFlags);
        }

        static void CommitCurrentTransaction(ULONG ulNumExpectedDiryBlocks = 1)
        {
            bool	bContinue = false;
            ULONG	ulNumDataBlocks = 0;
            ULONG	ulNumTags = 0;
            ULONG ulNumDrainedDiryBlocks = 0;
            do
            {
                s_commitTrans.ulTransactionID.QuadPart = 0;
                s_pIoctlController->GetDirtyBlockTransaction(&s_commitTrans, &s_uDirtyBlockV2, true, s_pSrcDisk->GetDeviceId());
                Assert::IsFalse(s_uDirtyBlockV2.uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED);

                if (TEST_FLAG(s_uDirtyBlockV2.uHdr.Hdr.ulFlags, UDIRTY_BLOCK_FLAG_SVD_STREAM)) {
                    Assert::IsTrue(0 != s_uDirtyBlockV2.uHdr.Hdr.ulcbChangesInStream);
                    Assert::IsTrue(0 != s_uDirtyBlockV2.uHdr.Hdr.usNumberOfBuffers);
                    Assert::IsTrue(NULL != s_uDirtyBlockV2.uHdr.Hdr.ppBufferArray);
                    Assert::IsTrue(0 != s_uDirtyBlockV2.uHdr.Hdr.ulBufferSize);
                }
                else if (!TEST_FLAG(s_uDirtyBlockV2.uHdr.Hdr.ulFlags, UDIRTY_BLOCK_FLAG_TSO_FILE))
                {
                    if (0 == s_uDirtyBlockV2.uHdr.Hdr.cChanges) {
                        ProcessTags(&s_uDirtyBlockV2);
                        +ulNumTags;
                    }
                    else {
                        ++ulNumDataBlocks;
                        for (unsigned long l = 0; l < s_uDirtyBlockV2.uHdr.Hdr.cChanges; l++) {
                            Assert::IsTrue(0 != s_uDirtyBlockV2.ChangeLengthArray[l]);
                        }
                    }
                }

                s_commitTrans.ulTransactionID = s_uDirtyBlockV2.uHdr.Hdr.uliTransactionID;
                s_pIoctlController->CommitTransaction(&s_commitTrans);
                bContinue = (WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)s_notifyEvent.Self(), 10));
                ulNumDrainedDiryBlocks++;
            } while (bContinue);

            Assert::IsTrue(ulNumDrainedDiryBlocks == ulNumExpectedDiryBlocks);
        }

        static void ProcessTags(PUDIRTY_BLOCK_V2 udbp)
        {
            PSTREAM_REC_HDR_4B pTag = &udbp->uTagList.TagList.TagEndOfList;
            std::list<string>        taglist;

            SV_ULONG   ulBytesProcessed = 0;
            SV_ULONG   ulNumberOfUserDefinedTags = 0;

            while (pTag->usStreamRecType != STREAM_REC_TYPE_END_OF_TAG_LIST)
            {
                SV_ULONG ulLength = STREAM_REC_SIZE(pTag);
                SV_ULONG ulHeaderLength = (pTag->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) ? sizeof(STREAM_REC_HDR_8B) : sizeof(STREAM_REC_HDR_4B);

                if (pTag->usStreamRecType == STREAM_REC_TYPE_USER_DEFINED_TAG)
                {
                    SV_ULONG    ulTagLength = 0;
                    char*       pTagData;

                    ulNumberOfUserDefinedTags++;

                    ulTagLength = *(SV_PUSHORT)((SV_PUCHAR)pTag + ulHeaderLength);

                    // This is tag name
                    pTagData = (char*)pTag + ulHeaderLength + sizeof(SV_USHORT);

                    taglist.push_back(pTagData);
                }

                ulBytesProcessed += ulLength;
                pTag = (PSTREAM_REC_HDR_4B)((SV_PUCHAR)pTag + ulBytesProcessed);
            }
        }

    };
}

CInmageIoctlController*             DataNotifyEventTests::DataNotifyEventTests::s_pIoctlController = NULL;
IPlatformAPIs*                      DataNotifyEventTests::DataNotifyEventTests::s_platformApis;
CLogger*                            DataNotifyEventTests::DataNotifyEventTests::s_pLogger;
IBlockDevice*                       DataNotifyEventTests::DataNotifyEventTests::s_pSrcDisk = NULL;
UDIRTY_BLOCK_V2                        DataNotifyEventTests::DataNotifyEventTests::s_uDirtyBlockV2 = { 0 };
COMMIT_TRANSACTION                    DataNotifyEventTests::DataNotifyEventTests::s_commitTrans = { 0 };
std::vector<char>                   DataNotifyEventTests::DataNotifyEventTests::s_buffer(24 * 1024 * 1024);
Event                               DataNotifyEventTests::DataNotifyEventTests::s_notifyEvent(false, true, std::string());