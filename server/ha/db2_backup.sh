ifconfig | grep ipaddress
if [ $? = "0" ]
then
    mysqldump svsdb1 > /home/svsystems/bin/db2/db.bak
fi
