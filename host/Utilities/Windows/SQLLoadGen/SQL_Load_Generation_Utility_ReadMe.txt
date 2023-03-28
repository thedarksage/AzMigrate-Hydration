4:37 PM 4/22/2011

Objective: To generate load on the SQL server by writing records in a table of SQL database.

Step:1 Open Command Window
Step:2 Run the following command

sqlcmd -S <sql instance> -d <database name> -i <path of the sql script file> -E

e.g:
sqlcmd -S pj-w2k3r2-v2 -d MMTEST -i E:\MMTEST\SQLLoadGen.sql -E



Observations:
For every 5th record, you will find a new record being inserted into the database with timestamp down to the millisecond level in the first column and record count in the remaining columns.