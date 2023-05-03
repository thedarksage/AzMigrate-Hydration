#! /bin/sh

## Incase of CentOS 6.2 'server-status' block is uncommented by default. No need to anyhthing.
## Incase of CentOS 5.5 'server-status' block is commented by default. Need to uncomment below lines.
#<Location /server-status>
#SetHandler server-status
#</Location>

sed -n '/#<Location \/server-status>/,/#<\/Location>/ {
=
d
}' /etc/httpd/conf/httpd.conf > sys_mon
for file in `cat sys_mon`
do
     variable=`sed -n "${file}p" /etc/httpd/conf/httpd.conf`

     if [ "$variable" = '#<Location /server-status>' ]; then
            sed -i -e "s|#<Location \/server-status>|<Location /server-status>|" /etc/httpd/conf/httpd.conf
     fi

     if [ "$variable" = '#</Location>' ]; then
            sed -i -e "$file s|#</Location>|</Location>|" /etc/httpd/conf/httpd.conf
     fi
done
rm -f sys_mon
