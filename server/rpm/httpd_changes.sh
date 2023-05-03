#!/bin/sh

# Commenting out some modules in the httpd.conf file to reduce the memory usage of the httpd service.
# Commenting out the following modules for improving the performance

#LoadModule ldap_module modules/mod_ldap.so
#LoadModule authnz_ldap_module modules/mod_authnz_ldap.so
#LoadModule logio_module modules/mod_logio.so
#LoadModule ext_filter_module modules/mod_ext_filter.so
#LoadModule usertrack_module modules/mod_usertrack.so
#LoadModule dav_module modules/mod_dav.so
#LoadModule info_module modules/mod_info.so
#LoadModule dav_fs_module modules/mod_dav_fs.so
#LoadModule actions_module modules/mod_actions.so
#LoadModule speling_module modules/mod_speling.so
#LoadModule rewrite_module modules/mod_rewrite.so
#LoadModule proxy_ftp_module modules/mod_proxy_ftp.so
#LoadModule proxy_balancer_module modules/mod_proxy_balancer.so
#LoadModule suexec_module modules/mod_suexec.so
#LoadModule cgi_module modules/mod_cgi.so
#LoadModule version_module modules/mod_version.so
#LoadModule userdir_module modules/mod_userdir.so
#LoadModule disk_cache_module modules/mod_disk_cache.so
#LoadModule file_cache_module modules/mod_file_cache.so
#LoadModule mem_cache_module modules/mod_mem_cache.so
#LoadModule authn_dbm_module modules/mod_authn_dbm.so
#LoadModule authz_dbm_module modules/mod_authz_dbm.so
#LoadModule authz_default_module modules/mod_authz_default.so
#LoadModule env_module modules/mod_env.so

sed -i -e "s/^LoadModule ldap_module modules\/mod_ldap.so/#LoadModule ldap_module modules\/mod_ldap.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule authnz_ldap_module modules\/mod_authnz_ldap.so/#LoadModule authnz_ldap_module modules\/mod_authnz_ldap.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule logio_module modules\/mod_logio.so/#LoadModule logio_module modules\/mod_logio.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule ext_filter_module modules\/mod_ext_filter.so/#LoadModule ext_filter_module modules\/mod_ext_filter.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule usertrack_module modules\/mod_usertrack.so/#LoadModule usertrack_module modules\/mod_usertrack.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule dav_module modules\/mod_dav.so/#LoadModule dav_module modules\/mod_dav.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule info_module modules\/mod_info.so/#LoadModule info_module modules\/mod_info.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule dav_fs_module modules\/mod_dav_fs.so/#LoadModule dav_fs_module modules\/mod_dav_fs.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule actions_module modules\/mod_actions.so/#LoadModule actions_module modules\/mod_actions.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule speling_module modules\/mod_speling.so/#LoadModule speling_module modules\/mod_speling.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule rewrite_module modules\/mod_rewrite.so/#LoadModule rewrite_module modules\/mod_rewrite.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule proxy_ftp_module modules\/mod_proxy_ftp.so/#LoadModule proxy_ftp_module modules\/mod_proxy_ftp.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule suexec_module modules\/mod_suexec.so/#LoadModule suexec_module modules\/mod_suexec.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule cgi_module modules\/mod_cgi.so/#LoadModule cgi_module modules\/mod_cgi.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule version_module modules\/mod_version.so/#LoadModule version_module modules\/mod_version.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule proxy_balancer_module modules\/mod_proxy_balancer.so/#LoadModule proxy_balancer_module modules\/mod_proxy_balancer.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule userdir_module modules\/mod_userdir.so/#LoadModule userdir_module modules\/mod_userdir.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule disk_cache_module modules\/mod_disk_cache.so/#LoadModule disk_cache_module modules\/mod_disk_cache.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule file_cache_module modules\/mod_file_cache.so/#LoadModule file_cache_module modules\/mod_file_cache.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule mem_cache_module modules\/mod_mem_cache.so/#LoadModule mem_cache_module modules\/mod_mem_cache.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule authn_dbm_module modules\/mod_authn_dbm.so/#LoadModule authn_dbm_module modules\/mod_authn_dbm.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule authz_dbm_module modules\/mod_authz_dbm.so/#LoadModule authz_dbm_module modules\/mod_authz_dbm.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule authz_default_module modules\/mod_authz_default.so/#LoadModule authz_default_module modules\/mod_authz_default.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/^LoadModule env_module modules\/mod_env.so/#LoadModule env_module modules\/mod_env.so/g" /etc/httpd/conf/httpd.conf

sed -i -e "s/#LoadModule status_module modules\/mod_status.so/LoadModule status_module modules\/mod_status.so/g" /etc/httpd/conf/httpd.conf

sed -i -e 's|.*LoadModule cgi_module modules/mod_cgi.so|LoadModule cgi_module modules/mod_cgi.so|g' /etc/httpd/conf/httpd.conf

sed -i -e "s|.*ScriptAlias /cgi-bin/.*|ScriptAlias /cgi-bin/ \"/home/svsystems/admin/web/cgi-bin/\"|g" /etc/httpd/conf/httpd.conf

sed -i -e 's|.*AddHandler cgi-script.*|AddHandler cgi-script .cgi|g' /etc/httpd/conf/httpd.conf

sed -i -e "{/.*<Directory \"\/var\/www\/cgi-bin\">/,/<\/Directory>/ s/#//g; s|<Directory \"/var/www/cgi-bin\">|<Directory \"/home/svsystems/admin/web/cgi-bin\">|g}" /etc/httpd/conf/httpd.conf

./http_sys_monitor.sh

sed -i -e "s|#.*SetHandler server-status|   SetHandler server-status|" /etc/httpd/conf/httpd.conf

sed -i -e "s|#ExtendedStatus On|ExtendedStatus On|" /etc/httpd/conf/httpd.conf
