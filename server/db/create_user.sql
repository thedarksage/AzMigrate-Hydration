use mysql;
drop database if exists svsdb1;
create database svsdb1;

GRANT ALL PRIVILEGES ON svsdb1.* TO 'svsystems'@'localhost' IDENTIFIED BY 'user-defined' WITH GRANT OPTION;FLUSH PRIVILEGES;
