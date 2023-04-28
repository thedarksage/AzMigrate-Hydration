ifconfig | grep ipaddress
if [ $? = "0" ]
then
    mysqldump svsdb1 > /home/svsystems/bin/db1/db.bak
fi
