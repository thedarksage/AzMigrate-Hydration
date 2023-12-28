Summary: Inmage Volume Replication Agent
Name: InMageVx
Version: %{_version}
Release: 1
License: commercial
Group: Applications/System
Prefix: /InMage/Vx
BuildRoot:%{_topdir}/%{name}-buildroot
Autoreq: 0
%define _use_internal_dependency_generator 0

%description
This is Inmage Vx Agent

# Cleaning after building RPM
%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
/InMage/Vx/scripts/clearwritesplits.pl
/InMage/Vx/scripts/inm_device_refresh.sh
/InMage/Vx/scripts/Array/arrayManager.pl
/InMage/Vx/scripts/Array/Log.pm
/InMage/Vx/scripts/Array/pillarArrayManager.pm
/InMage/Vx/bin/boot.inmage
/InMage/Vx/bin/boot.inmage.sles11
/InMage/Vx/bin/AzureRcmCli
/InMage/Vx/bin/AzureRecoveryUtil
/InMage/Vx/bin/cdpcli
/InMage/Vx/bin/cxcli
/InMage/Vx/bin/dataprotection
/InMage/Vx/bin/DataProtectionSyncRcm
/InMage/Vx/bin/evtcollforw
/InMage/Vx/bin/hostconfigcli
/InMage/Vx/bin/halt.local.rh
/InMage/Vx/bin/inmaged.modules
/InMage/Vx/bin/killchildren
/InMage/Vx/bin/s2A2A
/InMage/Vx/bin/s2V2A
/InMage/Vx/bin/ScoutTuning
/InMage/Vx/bin/start
/InMage/Vx/bin/status
/InMage/Vx/bin/stop
/InMage/Vx/bin/TuneMaxRep.sh
/InMage/Vx/bin/svagentsA2A
/InMage/Vx/bin/svagentsV2A
/InMage/Vx/bin/inm_list_part
/InMage/Vx/bin/vsnap_export_unexport.pl
/InMage/Vx/bin/uninstall
/InMage/Vx/bin/unregagent
/InMage/Vx/bin/unregister
/InMage/Vx/bin/vacp
/InMage/Vx/bin/EsxUtil
/InMage/Vx/bin/vol_pack.sh
/InMage/Vx/bin/collect_support_materials.sh
/InMage/Vx/bin/vxagent
/InMage/Vx/bin/vxagent.service.systemd
/InMage/Vx/bin/uarespawnd
/InMage/Vx/bin/uarespawnd.service.systemd
/InMage/Vx/bin/uarespawndagent
/InMage/Vx/bin/inmshutnotify
/InMage/Vx/bin/inm_dmit
/InMage/Vx/bin/genpassphrase
/InMage/Vx/bin/gencert
/InMage/Vx/bin/csgetfingerprint
/InMage/Vx/bin/check_drivers.sh
/InMage/Vx/bin/UnifiedAgentConfigurator.sh
/InMage/Vx/bin/teleAPIs.sh
/InMage/Vx/bin/jq
/InMage/Vx/bin/at_lun_masking_udev.sh
/InMage/Vx/bin/at_lun_masking.sh
/InMage/Vx/bin/09-inm_udev.rules
/InMage/Vx/etc/drscout.conf
/InMage/Vx/etc/RCMInfo.conf
/InMage/Vx/etc/notallowed_for_mountpoints.dat
/InMage/Vx/etc/inmexec_jobspec.txt
/InMage/Vx/scripts/application.sh
/InMage/Vx/scripts/dns.sh
/InMage/Vx/scripts/mysql.sh
/InMage/Vx/scripts/mysqltargetdiscovery.sh
/InMage/Vx/scripts/oracle.sh
/InMage/Vx/scripts/oracletargetdiscovery.sh
/InMage/Vx/scripts/postgresbegin.sql
/InMage/Vx/scripts/postgresconsistency.sh
/InMage/Vx/scripts/postgresdiscover.sh
/InMage/Vx/scripts/postgresend.sql
/InMage/Vx/scripts/samplefiles/sample_discovery.xml
/InMage/Vx/scripts/oracleappagent.sh
/InMage/Vx/scripts/oracleappdiscovery.sh
/InMage/Vx/scripts/oracleappdiscovery.pl
/InMage/Vx/scripts/oracle_consistency.sh
/InMage/Vx/scripts/oraclefailover.sh
/InMage/Vx/scripts/oracleappwizard.sh
/InMage/Vx/scripts/oraclediscovery.sh
/InMage/Vx/scripts/db2_consistency.sh
/InMage/Vx/scripts/db2discovery.sh
/InMage/Vx/scripts/db2appagent.sh
/InMage/Vx/scripts/db2failover.sh
/InMage/Vx/scripts/inmvalidator.sh
/InMage/Vx/scripts/vCon/*.sh
/InMage/Vx/scripts/vCon/vConService_linux
/InMage/Vx/scripts/vCon/vCon.service.systemd
/InMage/Vx/transport/newcert.pem
/InMage/Vx/transport/client.pem
/InMage/Vx/transport/cxps
/InMage/Vx/transport/cxpscli
/InMage/Vx/transport/cxps.conf
/InMage/Vx/transport/cxpsctl
/InMage/Vx/bin/cxpsclient
/InMage/Vx/bin/appservice
/InMage/Vx/bin/FailoverCommandUtil
/InMage/Vx/bin/inm_scsi_id
/InMage/Vx/bin/inmexec
/InMage/Vx/bin/boottimemirroring.sh
/InMage/Vx/bin/scsi_id.sh
/InMage/Vx/bin/boottimemirroring_phase1
/InMage/Vx/bin/boottimemirroring_phase2
/InMage/Vx/bin/generate_device_name
/InMage/Vx/bin/boot-involflt.sh
/InMage/Vx/bin/involflt.sh
/InMage/Vx/bin/get_protected_dev_details.sh
/InMage/Vx/bin/involflt_start.service.systemd
/InMage/Vx/bin/involflt_stop.service.systemd
/InMage/Vx/scripts/Cloud/AWS/aws_recover.sh
/InMage/Vx/scripts/azure/ifcfg-eth0
/InMage/Vx/scripts/azure/network
/InMage/Vx/scripts/azure/OS_details_target.sh
/InMage/Vx/scripts/azure/post-rec-script.sh
/InMage/Vx/scripts/azure/suse_ifcfg_eth0
/InMage/Vx/scripts/azure/setazureip.sh
/InMage/Vx/scripts/azure/prepareforazure.sh
/InMage/Vx/scripts/initrd/*
/InMage/Vx/bin/libcommon.sh

%if "%_vendor"=="redhat"
  %if %{?_with_RHEL5_64:1}%{!?_with_RHEL5_64:0}
     /InMage/Vx/bin/involflt.ko.2.6.18-194.el5
     /InMage/Vx/bin/involflt.ko.2.6.18-194.el5xen
  %else
  %if %{?_with_RHEL6_64:1}%{!?_with_RHEL6_64:0}
     /InMage/Vx/bin/cachemgr
     /InMage/Vx/bin/cdpmgr
     /InMage/Vx/bin/involflt.ko.2.6.32-71.el6.x86_64
     /InMage/Vx/bin/involflt.ko.2.6.32-71.el6.centos.plus.x86_64
  %else
  %if %{?_with_RHEL7_64:1}%{!?_with_RHEL7_64:0}
     /InMage/Vx/bin/cachemgr
     /InMage/Vx/bin/cdpmgr
     /InMage/Vx/bin/involflt.ko.3.10.0-123.el7.x86_64
     /InMage/Vx/bin/involflt.ko.3.10.0-514.el7.x86_64
     /InMage/Vx/bin/involflt.ko.3.10.0-693.el7.x86_64
  %else
  %if %{?_with_RHEL8_64:1}%{!?_with_RHEL8_64:0}
     /InMage/Vx/bin/involflt.ko.4.18.0-80.el8.x86_64
     /InMage/Vx/bin/involflt.ko.4.18.0-147.el8.x86_64
     /InMage/Vx/bin/involflt.ko.4.18.0-305.el8.x86_64
     /InMage/Vx/bin/involflt.ko.4.18.0-348.el8.x86_64
     /InMage/Vx/bin/involflt.ko.4.18.0-425.3.1.el8.x86_64
     /InMage/Vx/bin/involflt.ko.4.18.0-425.10.1.el8_7.x86_64
  %else
  %if %{?_with_RHEL9_64:1}%{!?_with_RHEL9_64:0}
     /InMage/Vx/bin/involflt.ko.5.14.0-70.13.1.0.3.el9_0.x86_64
     /InMage/Vx/bin/involflt.ko.5.14.0-70.36.1.el9_0.x86_64
     /InMage/Vx/bin/involflt.ko.5.14.0-162.6.1.el9_1.x86_64
     /InMage/Vx/bin/involflt.ko.5.14.0-284.11.1.el9_2.x86_64
  %else
  %if %{?_with_OL6_64:1}%{!?_with_OL6_64:0}
     /InMage/Vx/bin/involflt.ko.2.6.32-131.0.15.el6.x86_64
     /InMage/Vx/bin/involflt.ko.2.6.32-100.28.5.el6.x86_64
     /InMage/Vx/bin/involflt.ko.2.6.32-100.34.1.el6uek.x86_64
     /InMage/Vx/bin/involflt.ko.2.6.32-200.16.1.el6uek.x86_64
     /InMage/Vx/bin/involflt.ko.2.6.32-300.3.1.el6uek.x86_64
     /InMage/Vx/bin/involflt.ko.2.6.32-400.26.1.el6uek.x86_64
     /InMage/Vx/bin/involflt.ko.2.6.39-100.5.1.el6uek.x86_64
     /InMage/Vx/bin/involflt.ko.2.6.39-200.24.1.el6uek.x86_64
     /InMage/Vx/bin/involflt.ko.2.6.39-300.17.1.el6uek.x86_64
     /InMage/Vx/bin/involflt.ko.2.6.39-400.17.1.el6uek.x86_64
     /InMage/Vx/bin/involflt.ko.3.8.13-16.2.1.el6uek.x86_64
     /InMage/Vx/bin/involflt.ko.4.1.12-32.1.2.el6uek.x86_64
  %else
  %if %{?_with_OL7_64:1}%{!?_with_OL7_64:0}
     /InMage/Vx/bin/drivers/involflt.ko.*
  %else
  %if %{?_with_OL8_64:1}%{!?_with_OL8_64:0}
     /InMage/Vx/bin/drivers/involflt.ko.*
  %else
  %if %{?_with_OL9_64:1}%{!?_with_OL9_64:0}
     /InMage/Vx/bin/drivers/involflt.ko.5.14.0-70.13.1.0.3.el9_0.x86_64
     /InMage/Vx/bin/drivers/involflt.ko.5.14.0-70.36.1.el9_0.x86_64
     /InMage/Vx/bin/drivers/involflt.ko.5.15.0-0.30.19.el9uek.x86_64
     /InMage/Vx/bin/drivers/involflt.ko.5.14.0-162.6.1.el9_1.x86_64
     /InMage/Vx/bin/drivers/involflt.ko.5.14.0-284.11.1.el9_2.x86_64
  %endif   
  %endif
  %endif
  %endif
  %endif
  %endif
  %endif
  %endif
  %endif
%endif

%if "%_vendor"=="suse"
  %if %{?_with_SLES11_SP3_64:1}%{!?_with_SLES11_SP3_64:0}
     /InMage/Vx/bin/involflt.ko.3.0.76-0.11-default
     /InMage/Vx/bin/involflt.ko.3.0.76-0.11-xen
  %else 
  %if %{?_with_SLES11_SP4_64:1}%{!?_with_SLES11_SP4_64:0} 
     /InMage/Vx/bin/involflt.ko.3.0.101-63-default 
     /InMage/Vx/bin/involflt.ko.3.0.101-63-xen 
  %else 
  %if %{?_with_SLES12_64:1}%{!?_with_SLES12_64:0} 
     /InMage/Vx/bin/drivers/involflt.ko.*
     /InMage/Vx/bin/drivers/supported_kernels
  %else 
  %if %{?_with_SLES15_64:1}%{!?_with_SLES15_64:0} 
     /InMage/Vx/bin/drivers/involflt.ko.*
     /InMage/Vx/bin/drivers/supported_kernels
  %endif
  %endif
  %endif
  %endif  
%endif
