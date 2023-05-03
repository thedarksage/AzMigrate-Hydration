%define INSTALL_DIR /home/svsystems
%define PERL_MODULE_PATH %{INSTALL_DIR}/pm
%define BIN_PATH %{INSTALL_DIR}/bin
%define ETC_PATH %{INSTALL_DIR}/etc
%define VAR_PATH %{INSTALL_DIR}/var
%define SERVICE_PATH %{_sysconfdir}/init.d
%define HA_PATH %{INSTALL_DIR}/pm/ConfigServer/HighAvailability
%define MIB_PATH /usr/share/snmp/mibs

Name:		%{cliname}CX
Release:	%{clirelease}
Version:	%{cliversion}

Packager:	InMageSystems <dl-build-engineering@inmage.net>
Vendor:		%{cliname}
License:	commercial
Group:		Applications/System
Summary:	%{cliname} Configuration & Process Server RPM

BuildRoot:	%{_tmppath}/%{cliname}%{version}-Common
Prefix:		%{INSTALL_DIR}

%description
The %{cliname} Configuration & Process Server RPM

%install
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/snmp
mkdir -p $RPM_BUILD_ROOT%SERVICE_PATH
mkdir -p $RPM_BUILD_ROOT%BIN_PATH
mkdir -p $RPM_BUILD_ROOT%BIN_PATH/db
mkdir -p $RPM_BUILD_ROOT%ETC_PATH
mkdir -p $RPM_BUILD_ROOT%VAR_PATH
mkdir -p $RPM_BUILD_ROOT%VAR_PATH/cli
mkdir -p $RPM_BUILD_ROOT%HA_PATH
mkdir -p $RPM_BUILD_ROOT%MIB_PATH
mkdir -p $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/app
mkdir -p $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/inmages
mkdir -p $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/cs/classes/conn
mkdir -p $RPM_BUILD_ROOT%INSTALL_DIR/transport
mkdir -p $RPM_BUILD_ROOT%INSTALL_DIR/transport/pem

# Conf files
cp -f tm/etc/* $RPM_BUILD_ROOT%ETC_PATH
cp -f linux/my.cnf $RPM_BUILD_ROOT%{_sysconfdir}
cp -f linux/snmpd.conf $RPM_BUILD_ROOT%{_sysconfdir}/snmp


# PHP files - these should be going away in a few days' time
cp -f admin/web/stop_replication_ps.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/
cp -f admin/web/resync_now.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/
cp -f admin/web/config.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/
cp -f admin/web/curl_dispatch_http.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/
cp -f admin/web/update_scenario_ps.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/

cp -f admin/web/ui/get_services_status.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web
cp -f admin/web/ui/get_perf_stats.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web
cp -f admin/web/inmages/health_holderRight.gif $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/inmages/
cp -f admin/web/inmages/health_red.gif $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/inmages/
cp -f admin/web/inmages/health_green.gif $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/inmages/
cp -f admin/web/inmages/health_yellow.gif $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/inmages/
cp -f admin/web/app/VolumeProtection.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/app
cp -f admin/web/app/CommonFunctions.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/app
cp -f admin/web/app/Logging.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/app
cp -f admin/web/app/Recovery.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/app
cp -f admin/web/app/Retention.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/app
cp -f admin/web/app/System.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/app
cp -f admin/web/app/BandwidthPolicy.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/app
cp -f admin/web/app/Constants.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/app

cp -f admin/web/cs/classes/conn/Connection.php $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/cs/classes/conn

# Programs
cp -f diffdatasort $RPM_BUILD_ROOT%BIN_PATH
cp -f inmtouch $RPM_BUILD_ROOT%BIN_PATH
cp -f svdcheck $RPM_BUILD_ROOT%BIN_PATH
cp -f tm/"db sync"/db*.sh $RPM_BUILD_ROOT%BIN_PATH/db
cp -f inmlicense $RPM_BUILD_ROOT%BIN_PATH
cp -f scheduler $RPM_BUILD_ROOT%BIN_PATH
cp -f csgetfingerprint $RPM_BUILD_ROOT%BIN_PATH
cp -f genpassphrase $RPM_BUILD_ROOT%BIN_PATH
cp -f viewcert $RPM_BUILD_ROOT%BIN_PATH
cp -f gencert $RPM_BUILD_ROOT%BIN_PATH
cp -f sslsign $RPM_BUILD_ROOT%BIN_PATH

# transport related files
cp -f transport/cxps $RPM_BUILD_ROOT%INSTALL_DIR/transport
cp -f transport/cxpscli $RPM_BUILD_ROOT%INSTALL_DIR/transport
cp -f transport/servercert.pem $RPM_BUILD_ROOT%INSTALL_DIR/transport/pem
cp -f transport/serverkey.pem $RPM_BUILD_ROOT%INSTALL_DIR/transport/pem
cp -f transport/dh.pem $RPM_BUILD_ROOT%INSTALL_DIR/transport/pem
cp -f transport/cxps.conf $RPM_BUILD_ROOT%INSTALL_DIR/transport
cp -f transport/cxpsctl $RPM_BUILD_ROOT%SERVICE_PATH

# cxpsclient
cp -f ./cxpsclient $RPM_BUILD_ROOT%BIN_PATH

# Perl/Shell Scripts
cp -rf tm/bin/* $RPM_BUILD_ROOT%BIN_PATH
cp -f ha/db_sync_src_pre_script.pl $RPM_BUILD_ROOT%BIN_PATH
cp -f ha/db_sync_tgt_post_script.pl $RPM_BUILD_ROOT%BIN_PATH

# Perl Modules
cp -rf tm/pm/* $RPM_BUILD_ROOT%PERL_MODULE_PATH
cp -f linux/Abhai-1-MIB-V1.mib $RPM_BUILD_ROOT%MIB_PATH
cp -f tm/tmanagerd $RPM_BUILD_ROOT%SERVICE_PATH
cp -f tm/systemjobd $RPM_BUILD_ROOT%SERVICE_PATH

# Certificates
cp -f linux/newcert.pem $RPM_BUILD_ROOT%ETC_PATH
cp -f linux/newreq.pem $RPM_BUILD_ROOT%ETC_PATH
cp -f linux/cacert.pem $RPM_BUILD_ROOT%ETC_PATH

# Miscellaneous
cp -rf admin $RPM_BUILD_ROOT%INSTALL_DIR
cp -rf Cloud $RPM_BUILD_ROOT%INSTALL_DIR
rm -rf $RPM_BUILD_ROOT%INSTALL_DIR/admin/web/cli/doc

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%INSTALL_DIR
%SERVICE_PATH/tmanagerd
%SERVICE_PATH/systemjobd
%SERVICE_PATH/cxpsctl
%MIB_PATH/Abhai-1-MIB-V1.mib

# System files first
%{_sysconfdir}/my.cnf
%{_sysconfdir}/snmp/snmpd.conf
