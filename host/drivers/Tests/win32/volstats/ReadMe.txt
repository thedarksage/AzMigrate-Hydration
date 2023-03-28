========================================================================
    CONSOLE APPLICATION : volstats Project Overview
========================================================================
volstats collects volume statistics from the driver and writes them to a log file 
using a tab deliminated format that can then be imported into a spreadsheet application 
for review.

usage: volstats [-h] [-f <outfile>] [-s <read stats rate>]
  -h : diplays usage\n"
  -f <outfile>: file to write stats into. Default is volstats.log"
  -s <read stats rate>: number of seconds to wait between stats reads. default is 5 seconds"
   
press Ctrl-c to stop volstats

example of volstats raw output 
Note: since it is tab deliminated the column headers won't line up with the data when looking at the raw output.
Importing into a spreadsheet lines everything up.

Date	Total Volumes	Volumes Returned	Dirty Blocks Allocated	Service State
03/07/2005 17:27:02	3	3	0	2
Date	Volume GUID	Changes Returned To Service	Changes Read From Bit Map	Changes Written To Bit Map	Changes Queued For Writing	Changes Written To Bit Map	Changes Read From Bit Map	Changes Returned To Service	Changes Pending Commit	Changes Reverted	Pending Changes	Changes Pending Commit	Changes Queued For Writing	Dirty Blocks In Queue	Bit Map Write Errors	Volume Flags	Changes Reverted	Bit Map Read State
03/07/2005 17:27:07	b59b944f-8026-11d9-b9f8-806d6172696f	17480	112	0	0	0	1060864	17480	0	0	0	0	0	0	0	8	0	4	
03/07/2005 17:27:07	5ebd58ec-85f2-11d9-9afa-00016c2b4691	19237	5	0	0	0	45056	19237	0	0	0	0	6684777	0	0	8	0	4	
03/07/2005 17:27:07	5ebd58ed-85f2-11d9-9afa-00016c2b4691	19242	3	0	0	0	36864	19242	0	0	0	0	0	0	0	8	0	4	

  
  
  










