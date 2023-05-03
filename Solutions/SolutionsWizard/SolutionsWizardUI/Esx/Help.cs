using System;
using System.Collections.Generic;
using System.Text;
using Com.Inmage.Tools;

namespace Com.Inmage.Wizard
{

   
    public class HelpForcx
    {
        internal static int Brandingcode = WinTools.BrandingCode();
        internal static string VersionNumber = WinTools.VersionNumber().ToString();
        internal static string BuildDate = WinTools.BuildDateOfVcontinuum();
        internal static string P2vProtectScreen1 = "Select physical machines to be protected. Provide either local administrator or domain administrator credentials to login to the server. vContinuum will connect "
                                                 + "to the remote machine to collect the HW configuration. After the initial connection is established, you can unselect any disk, except disk0, that you do not want to protect.";

        internal static string ProtectScreen1 = "Select primary vSphere(ESX) host on which you have the VMs to be protected. "
                                                 + "In case of windows, you need to provide credentials to login to the server. vContinuum will connect "
                                                 + "to the remote machine to perform the readiness checks necessary to install agents remotely. "
                                                 + "In case of any WMI related errors you can choose to skip that step.";
        internal static string ProtectScreen2 = "Select secondary vSphere(ESX) host on to which you want to protect the primary VMs. "
                                                + "A VM has to be prepared prior hand to use as a Master target. "
                                                + "Replications will be set between primary VM(s) to the Mastertaget VM."
                                                + "At the time of recovery VMs would be recovered automatically on the secondary vSphere host. "
                                                + "Only one master target can be selected per protection.";
        internal static string ProtectScreen3 = "An agent has to be installed on each of the VMs before they can be protected. "
                                                 + "VContinuum can push agent to the remote VM and install it automatically. "
                                                 + "Machines for which you have selected to skip the ”Remote agent install” feature cannot be selected in this screen.";

        internal static string ProtectScreen4 = "Select the data stores on which you would like to create the VMs. " + Environment.NewLine
                                                 + "Select drives on master target to store the retention data"
                                                 + "Retention data can be configured based on the amount of space allocated or by the number of days worth of data to save."
                                                 + "Provide the schedule to generate the application consistency points";


        internal static string ProtectScreen5 = "Jobs necessary to create the replication will be created in this step. "
                                                 + "These jobs will collect the necessary information about the primary servers and sets the replication between the primary servers and the master target."
                                                 + "1.Run readiness checks Readiness checks make sure that all VMs are ready for protection. "+ Environment.NewLine
                                                 + "2.Once the readiness checks pass, provide plan name. Jobs will be set under this plan name. " + Environment.NewLine
                                                 + "After this step is complete, log in to CX UI and check for the progress of the jobs. "
                                                 + "Jobs can be seen in Monitor->Plan Name section ";

        internal static string ProtectScreen6 = "Network settings of selected VMs can be configured."
                                                + "Network Settings shows the MAC address of NIC card used by VM(s). Click on Change to provide the new ip an other details. "
                                                + "NIC configuration screen you can choose Target vSphere Network Name from the drop-downlist or you can enter any Network name"
                                                + "Under Hardware Settings, You can change Number virtual CPUs and Size of Memory for protected VM(s) on target vSphere.";




        internal static string RemoveScreen1 = "Remove feature is used to remove one or more VMs that were protected earlier without doing either recovery or failback. "
                                                  + "This operation is generally used in cases where you want to cleanup old entries and start a fresh protection." + Environment.NewLine
                                                  + "Remove operation 1.Removes jobs and replication pairs created in CX "
                                                  + "2.Removes VM entry from the plan name " 
                                                  + "3.Detaches disks from Master Target so that VM can be removed." 
                                                  + "VM has to be manually deleted either using vCenter or vShpere client.";

        internal static string RecoveryScreen1 = "Recover is used to rollback secondary VM(s) to either prior application consistency point or to a prior point in time. " + Environment.NewLine
                                                  + "You can recover a VM to " + Environment.NewLine
                                                  + "1.Latest application consistent point that is common across all volumes in that VM " + Environment.NewLine
                                                  + "2.Latest point in time " + Environment.NewLine
                                                  + "3.Consistency point near(prior to) any given time" + Environment.NewLine;



        internal static string OfflinesyncExport = "Offline sync feature enables you to send the copy of the data to secondary server via a removable media."
                                                    + Environment.NewLine + "Offline sync has following three steps"
                                                    + Environment.NewLine + "Step 1 Offline sync export"
                                                    + Environment.NewLine + "Step 2 Transfer folders to the removable media and copy them to the secondary vSphere server"
                                                    + Environment.NewLine + "Step 3 Run vContinuum wizard, choose Offline Sync->Import";

        internal static string OfflineSyncImport = "Step1.Copy files to a datastore on the secondary vSphere server from the removable media"
                                                   + Environment.NewLine + "Wizard searches for the folder named \"InMage_Offline_Sync_Folder\" on all data stores of secondary vSphere server"
                                                   + Environment.NewLine + "Step2: Select datastore where the master target VM is restored to"
                                                   + Environment.NewLine + "Wizard will import the master target and protected VMs.";

        internal static string AdditionofDisk1 = "This feature is used to add disks to an already protected VM."
                                                + Environment.NewLine + " Replication will be set only for the newly added disks"
                                                + Environment.NewLine + "Select protected VMs on secondary vSphere server";

        internal static string AdditionofDisk2 = Environment.NewLine + "Select the disks that you would like to add to the protection";



        internal static string Failback = "Failback operation replicates any new changes made on the secondary VM back to the primary VM."
                                        + "Failback can be done only on those VMs that are failed over to the secondary server."
                                        + "Replication pairs are set from secondary VMs running secondary vSphere to master target running"
                                        + "on primary vSphere server. We need to prepare a master target on the primary vSphere server before"
                                        + "starting the protection.";
        internal static string Drdrill3 = "Select the data stores on which you would like to create the Snapshot VMs. " + Environment.NewLine;

        internal static string Upgrade = "You can migrate from vSphere plans to vCenter or from older plans to latest version." + Environment.NewLine 
                                        +"You can migrate both source and target vSphere plans to vCenter plans.";
    }
}
