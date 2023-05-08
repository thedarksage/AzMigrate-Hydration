// vSpherePowerCLIlib.h

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <msclr\marshal_cppstd.h>

#pragma once

using namespace System;
using namespace System::Runtime::InteropServices;
using namespace VmWareClient;

namespace vSpherePowerCLIlib {

	public ref class PowerCLIClient
	{
	public: 
		PSVMwareClient^ psClient;
		Object^ vm;

		PowerCLIClient(String^ vCenterIP, String^ UserName, String^ Password)
		{
			// PSVMwareClient::InitializePSClient();
			psClient = gcnew PSVMwareClient(vCenterIP, UserName, Password);
		}

		void GetVmByName(String^ vmName)
		{
			vm = psClient->GetVmByName(vmName);
		}

		void CopyFileToVm(String^ filePathToBeCopied, String^ destinationFolder, String^ vmUserName, String^ vmPassword)
		{
			psClient->CopyFileToVm(filePathToBeCopied, destinationFolder, vmUserName, vmPassword, vm);
		}

		void PullFileFromVm(String^ filePathToBePulled, String^ destinationFolder, String^ vmUserName, String^ vmPassword)
		{
			psClient->PullFileFromVm(filePathToBePulled, destinationFolder, vmUserName, vmPassword, vm);
		}

		String^ InvokeScriptOnVm(String^ scriptToBeInvoked, String^ vmUserName, String^ vmPassword)
		{
			return psClient->InvokeScriptOnVm(scriptToBeInvoked, vmUserName, vmPassword, vm);
		}
	};
}

__declspec(dllexport) void* GetVmWareInstallerHandle(const std::string & vCenterIP, const std::string & UserName, const std::string & Password, int retries)
{
	bool done = false;
	int attempts = 0;
	while (!done)
	{
		try
		{
			vSpherePowerCLIlib::PowerCLIClient^ client = gcnew vSpherePowerCLIlib::PowerCLIClient(gcnew String(vCenterIP.c_str()), gcnew String(UserName.c_str()), gcnew String(Password.c_str()));
			done = true;
			return GCHandle::ToIntPtr(GCHandle::Alloc(client)).ToPointer();
		}
		catch (Exception^ ex)
		{
			attempts++;
			if (attempts > retries)
			{
				msclr::interop::marshal_context context;
				std::string error = context.marshal_as<std::string>(ex->ToString());
				throw new std::exception(error.c_str());
			}
		}
	}
}

__declspec(dllexport) void VmWareCheckVmConnectivity(void* vmWareHandle, const std::string & vmName, int retries)
{
	bool done = false;
	int attempts = 0;
	while (!done)
	{
		try
		{
			GCHandle h = GCHandle::FromIntPtr(IntPtr(vmWareHandle));
			vSpherePowerCLIlib::PowerCLIClient^ client = (vSpherePowerCLIlib::PowerCLIClient^)h.Target;
			client->GetVmByName(gcnew String(vmName.c_str()));
			done = true;
		}
		catch (Exception^ ex)
		{
			attempts++;
			if (attempts > retries)
			{
				msclr::interop::marshal_context context;
				std::string error = context.marshal_as<std::string>(ex->ToString());
				throw new std::exception(error.c_str());
			}
		}
	}
}

__declspec(dllexport) void VmWareCopyFileToRemoteVm(void* vmWareHandle, const std::string & filePathToBeCopied, const std::string & destinationFolder, const std::string & vmUserName, const std::string & vmPassword, int retries)
{
	bool done = false;
	int attempts = 0;
	while (!done)
	{
		try
		{
			GCHandle h = GCHandle::FromIntPtr(IntPtr(vmWareHandle));
			vSpherePowerCLIlib::PowerCLIClient^ client = (vSpherePowerCLIlib::PowerCLIClient^)h.Target;
			client->CopyFileToVm(
				gcnew String(filePathToBeCopied.c_str()),
				gcnew String(destinationFolder.c_str()),
				gcnew String(vmUserName.c_str()),
				gcnew String(vmPassword.c_str()));
			done = true;
		}
		catch (Exception^ ex)
		{
			attempts++;
			if (attempts > retries)
			{
				msclr::interop::marshal_context context;
				std::string error = context.marshal_as<std::string>(ex->ToString());
				throw new std::exception(error.c_str());
			}
		}
	}
}

__declspec(dllexport) void VmWarePullFileFromRemoteVm(void* vmWareHandle, const std::string & filePathToBePulled, const std::string & destinationFolder, const std::string & vmUserName, const std::string & vmPassword, int retries)
{
	bool done = false;
	int attempts = 0;
	while (!done)
	{
		try
		{
			GCHandle h = GCHandle::FromIntPtr(IntPtr(vmWareHandle));
			vSpherePowerCLIlib::PowerCLIClient^ client = (vSpherePowerCLIlib::PowerCLIClient^)h.Target;
			client->PullFileFromVm(
				gcnew String(filePathToBePulled.c_str()),
				gcnew String(destinationFolder.c_str()),
				gcnew String(vmUserName.c_str()),
				gcnew String(vmPassword.c_str()));
			done = true;
		}
		catch (Exception^ ex)
		{
			attempts++;
			if (attempts > retries)
			{
				msclr::interop::marshal_context context;
				std::string error = context.marshal_as<std::string>(ex->ToString());
				throw new std::exception(error.c_str());
			}
		}
	}
}

__declspec(dllexport) std::string VmWareInvokeScriptOnVm(void* vmWareHandle, const std::string & scriptToBeInvoked, const std::string & vmUserName, const std::string & vmPassword, int retries)
{
	bool done = false;
	int attempts = 0;
	while (!done)
	{
		try
		{
			GCHandle h = GCHandle::FromIntPtr(IntPtr(vmWareHandle));
			vSpherePowerCLIlib::PowerCLIClient^ client = (vSpherePowerCLIlib::PowerCLIClient^)h.Target;
			String^ output = client->InvokeScriptOnVm(
				gcnew String(scriptToBeInvoked.c_str()),
				gcnew String(vmUserName.c_str()),
				gcnew String(vmPassword.c_str()));
			done = true;

			msclr::interop::marshal_context context;
			return context.marshal_as<std::string>(output);
		}
		catch (Exception^ ex)
		{
			attempts++;
			if (attempts > retries)
			{
				msclr::interop::marshal_context context;
				std::string error = context.marshal_as<std::string>(ex->ToString());
				throw new std::exception(error.c_str());
			}
		}
	}
}

__declspec(dllexport) std::string VmWareGetVmOsName(void* vmWareHandle, int os_id, const std::string & scriptFileToBeInvoked, const std::string & destinationFolder, const std::string & vmUserName, const std::string & vmPassword, int retries)
{
	bool done = false;
	int attempts = 0;

	if (os_id == 1)
	{
		//Windows VM
		return "Win";
	}

	while (!done)
	{
		try
		{
			GCHandle h = GCHandle::FromIntPtr(IntPtr(vmWareHandle));
			vSpherePowerCLIlib::PowerCLIClient^ client = (vSpherePowerCLIlib::PowerCLIClient^)h.Target;
			client->CopyFileToVm(
				gcnew String(scriptFileToBeInvoked.c_str()),
				gcnew String(destinationFolder.c_str()),
				gcnew String(vmUserName.c_str()),
				gcnew String(vmPassword.c_str()));

			std::string scriptToBeInvoked = destinationFolder;

			std::string::size_type idx = scriptFileToBeInvoked.find_last_of("\\");
			if (std::string::npos != idx) {
				scriptToBeInvoked += scriptFileToBeInvoked.substr(idx + 1);
			}

			std::string executePermissionScript = "chmod 550 ";
			executePermissionScript += scriptToBeInvoked;
			client->InvokeScriptOnVm(
				gcnew String(executePermissionScript.c_str()),
				gcnew String(vmUserName.c_str()),
				gcnew String(vmPassword.c_str()));

			scriptToBeInvoked += " 1";

			String^ output = client->InvokeScriptOnVm(
				gcnew String(scriptToBeInvoked.c_str()),
				gcnew String(vmUserName.c_str()),
				gcnew String(vmPassword.c_str()));

			msclr::interop::marshal_context context;
			std::string osName = context.marshal_as<std::string>(output);
			if (osName == "RHEL6U4-64") {
				osName = "RHEL6-64";
			}

			done = true;
			return osName;
		}
		catch (Exception^ ex)
		{
			attempts++;
			if (attempts > retries)
			{
				msclr::interop::marshal_context context;
				std::string error = context.marshal_as<std::string>(ex->ToString());
				throw new std::exception(error.c_str());
			}
		}
	}
}
