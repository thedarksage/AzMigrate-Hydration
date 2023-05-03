#include "stdafx.h"
#include "CppUnitTest.h"

#include "S2Agent.h"
using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace S2AgentTest
{
	DWORD WINAPI GenerateWrites(_In_ LPVOID lpParameter)
	{

		CDiskDevice* disk1 = new CDiskDevice(1, new CWin32APIs());
		CDiskDevice* disk2 = new CDiskDevice(2, new CWin32APIs());

		char* buffer = new char[512];
		ZeroMemory(buffer, 512);

		uint64_t counter = 1;
		UINT64 sectorOffset = 0;

		disk1->SeekFile(BEGIN, 1);
		disk2->SeekFile(BEGIN, 1);

		UINT64	maxNumSectors = disk1->GetDeviceSize() / 512;
		UINT64	byteOffset = 512 - sizeof(UINT64);

		while (1)
		{
			sectorOffset++;
			if ((0 == sectorOffset) || (sectorOffset == maxNumSectors))
			{
				sectorOffset = 1;
			}
			disk1->SeekFile(BEGIN, sectorOffset);
			disk2->SeekFile(BEGIN, sectorOffset);

			DWORD dwBytes = 0;
			memcpy(&buffer[byteOffset], &counter, sizeof(counter));


			disk1->Write(buffer, 512, dwBytes);
			disk2->Write(buffer, 512, dwBytes);
			++counter;
			Sleep(30 * 1000);
		}
	}

	TEST_CLASS(UnitTest1)
	{
	public:
		
		TEST_METHOD(TestMethod1)
		{
			// TODO: Your test code here
			CS2Agent* agent = new CS2Agent(1, "c:\\tmp\\WalTest1");

			agent->AddReplicationSettings(1, -1, "c:\\tmp\\PhyscialDrive1.log");
			agent->AddReplicationSettings(2, -1, "c:\\tmp\\PhyscialDrive2.log");

			agent->StartReplication(0, 1024, true);

			DWORD dwThreadId;
			HANDLE h = CreateThread(NULL, 0, GenerateWrites, NULL, 0, &dwThreadId);

			for (int i = 0; i < 10; i++)
			{
				list<string> taglist;
				taglist.push_back("test1");
				agent->InsertCrashTag("test1", taglist);
				Sleep(3 * 60 * 1000);
			}

			WaitForSingleObject(h, INFINITE);
		}

		
	};
}