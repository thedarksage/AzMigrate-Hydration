# Build & Test Status
| Platform | Build |
|---|:-----:|
|![Win-x64](docs/img/win_logo.png) ***Windows x64***|[![Build Status](https://msazure.visualstudio.com/One/_apis/build/status/OneBranch/AzMigrate-Hydration/thedarksage.AzMigrate-Hydration?branchName=develop)](https://msazure.visualstudio.com/One/_build/latest?definitionId=281574&branchName=main)
|![Ubuntu-20.04-x64](docs/img/linux_logo.png) ***Ubuntu 20.04***|[![Build Status](https://msazure.visualstudio.com/One/_apis/build/status/OneBranch/AzMigrate-Hydration/thedarksage.AzMigrate-Hydration?branchName=develop)](https://msazure.visualstudio.com/One/_build/latest?definitionId=281574&branchName=main)

| Platform | Test |
|---|:-----:|
|![Win-x64](docs/img/win_logo.png) ***Windows 2016 Datacenter***|[![Build Status](https://msazure.visualstudio.com/One/_apis/build/status/OneBranch/AzMigrate-Hydration/thedarksage.AzMigrate-Hydration?branchName=develop)](https://msazure.visualstudio.com/One/_build/latest?definitionId=281574&branchName=main)
|![Redhat7-x64](docs/img/redhat_logo.png) ***Redhat 7.7***|[![Build Status](https://msazure.visualstudio.com/One/_apis/build/status/OneBranch/AzMigrate-Hydration/thedarksage.AzMigrate-Hydration?branchName=develop)](https://msazure.visualstudio.com/One/_build/latest?definitionId=281574&branchName=main)
|![CentOS7-x64](docs/img/centos_logo.png) ***CentOS 7.0***|[![Build Status](https://msazure.visualstudio.com/One/_apis/build/status/OneBranch/AzMigrate-Hydration/thedarksage.AzMigrate-Hydration?branchName=develop)](https://msazure.visualstudio.com/One/_build/latest?definitionId=281574&branchName=main)
|![SLES15-x64](docs/img/opensuse_logo.png) ***SLES 15 SP2***|[![Build Status](https://msazure.visualstudio.com/One/_apis/build/status/OneBranch/AzMigrate-Hydration/thedarksage.AzMigrate-Hydration?branchName=develop)](https://msazure.visualstudio.com/One/_build/latest?definitionId=281574&branchName=main)
|![Ubuntu-18.04-x64](docs/img/ubuntu_logo.png) ***Ubuntu 18.04***|[![Build Status](https://msazure.visualstudio.com/One/_apis/build/status/OneBranch/AzMigrate-Hydration/thedarksage.AzMigrate-Hydration?branchName=develop)](https://msazure.visualstudio.com/One/_build/latest?definitionId=281574&branchName=main)

# Project
This project provides the source code for [Azure Migrate hydration scripts](https://docs.microsoft.com/en-us/azure/migrate/prepare-for-agentless-migration#hydration-process).
Hydration provides the capability to patch a VM disk brought onto azure with the necessary configuration changes to make the VM bootable on Azure, and necessary adjustments made > for VM to be acquire a connectable state on Azure platform. Hydration aditionally makes confiuration changes to make VM Azure environment friendly by redirectly the console output to Azure's [Serial Console](https://docs.microsoft.com/en-us/troubleshoot/azure/virtual-machines/serial-console-overview), and install the [VM Guest agent](https://docs.microsoft.com/en-us/azure/virtual-machines/extensions/agent-linux), required for VM extensions and post deployment configuration of a VM.

# Requirements
The scripts available in this repository can only be used through Powershell with [Az Module](https://docs.microsoft.com/en-us/powershell/azure/install-az-ps?view=azps-7.2.0#:~:text=%20In%20those%20situations%2C%20you%20can%20install%20the,manually%20copy%20it%20to%20other%20machines.%20More%20) installed.

The script doesn't copy contents from OnPrem hypervisor VMs to an Azure Managed disk. It expects customers to have an actiove subscription on azure with the disk contents present on an Azure managed disk.

For a Linux VM which is originally based out of Hyper-V hypervisor, it is possible for the VM to have [Hyper-V LIS components](https://docs.microsoft.com/en-us/windows-server/virtualization/hyper-v/supported-centos-and-red-hat-enterprise-linux-virtual-machines-on-hyper-v) missing. Before bringing the disk to Azure, it is expected for these components to be installed on the VMWare/Other hypervisors based VMs for hydration to work properly.

Some features may require additional internal tools such as:
1. dracut/mkinitrd, lsinitrd in Linux VMs if Hyper-V LIS modules are missing in kernel image.
2. dhclient/dhcpcd in Linux VMs if networking rules are not configured to DHCP.
3. Python 2.6+ for installation of Linux Guest Agent. [Read additional basic requirements here](https://docs.microsoft.com/en-us/azure/virtual-machines/extensions/agent-linux#requirements). It's preferred to have setuptools appropriate for the python version installed on the OnPrem machine, but it's not a blocker.

## Usage

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft 
trademarks or logos is subject to and must follow 
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.
