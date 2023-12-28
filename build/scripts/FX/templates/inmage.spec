Summary: InMage Fx Agent
Name: InMageFx
Version: %{_version}
Release: 1
License: commercial
Group: Applications/System
Prefix: /InMage/Fx
BuildRoot: %{_topdir}/%{name}-buildroot
Autoreq: 0
%define _use_internal_dependency_generator 0

%description
This is InMage Fx Agent
%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/InMage/Fx/cxcli
/InMage/Fx/start
/InMage/Fx/stop
/InMage/Fx/svfrd
/InMage/Fx/inmsync
/InMage/Fx/status
/InMage/Fx/uninstall
/InMage/Fx/unregister
/InMage/Fx/svagent
/InMage/Fx/unregfragent
/InMage/Fx/config.ini
/InMage/Fx/killchildren
/InMage/Fx/linuxshare_*.sh
/InMage/Fx/vacp
/InMage/Fx/genpassphrase
/InMage/Fx/gencert
/InMage/Fx/csgetfingerprint
/InMage/Fx/fabric/scripts/*.sh
/InMage/Fx/scripts/*.sh
/InMage/Fx/cx_backup_withfx.sh
/InMage/Fx/transport/cxps
/InMage/Fx/transport/cxps.conf
/InMage/Fx/transport/pem/servercert.pem
/InMage/Fx/transport/pem/dh.pem
%if %{?_with_rhel3:1}%{!?_with_rhel3:0}
/InMage/Fx/scripts/ESX/*.pl
/InMage/Fx/lib/libstdc++.so.6
/InMage/Fx/lib/libgcc_s.so.1
%endif
