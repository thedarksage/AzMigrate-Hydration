#!/bin/bash

if [ $# -ne 1 ]; then
  echo "usage: $0 [days]"
  exit 1
fi

numdaystocheck=$1
compdate=`date -d "now -${numdaystocheck} days" +'%Y-%m-%d'`
logfile=`pwd`'/'`basename ${0}`-`date +'%s'`
outfile=`pwd`'/'NewKernels-`date +'%d_%b_%Y'`
numoutrecs=0

LOG()
{
 #echo $* >> $logfile
 return
}

OUT()
{
    if [ $numoutrecs -gt 0 ]; then
        echo "," >> $outfile
    fi
    echo -ne $* >> $outfile
    numoutrecs=`expr $numoutrecs + 1`
    return
}

LOG "compdate: $compdate"

> $outfile
echo -e "{\t\"NewKernels\" : [" >> $outfile

for ip in `cat /BUILDS/SCRIPTS/RELEASE/InMage-Azure-SiteRecovery/build/scripts/general/iplist`
do
    IP=`echo ${ip} | cut -d":" -f1`
    DISTRO=`echo ${ip} | cut -d":" -f2 | sed 's/-64$//g'`
    LOG "==============="
    LOG "Distro: $DISTRO, IPAddr: $IP"
    LOG "==============="
    echo $DISTRO | grep -i ubuntu >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        for kernfile in `ssh ${IP} "zgrep -i \"status installed\" /var/log/dpkg.log* | grep -v linux-headers-generic | grep linux-headers.*amd64" | sed 's/ /#/g'`
        do
            LOG "Installed headers: $kernfile"
       	    file_date=`echo $kernfile | awk -F# '{print $1}'  | awk -F: '{print $2}'`
            if [[ "$file_date" > "$compdate" ]]; then
                #echo "File date: $file_date"
                kern=`echo $kernfile | awk -F# '{print $(NF-1)}' | awk -F: '{print $1}' | sed 's/linux-headers-//g'`
             OUT "\t\t{ \"Distro\" : \"${DISTRO}\",\n\t\t \"Date\" : \"${file_date}\",\n\t\t \"Kernel\" : \"${kern}\"\n\t\t}"
            fi
        done
    else
        listLibMod=`ssh ${IP} "ls -ltr /lib/modules"`
	LOG $listLibMod
        for kernfile in `ssh ${IP} "find /lib/modules -maxdepth 1 -mtime -${numdaystocheck}"`
        do
             if [[ "$kernfile" == "/lib/modules" ]]; then
                 continue
             fi
             kerndate=`ssh ${IP} "stat -c %y $kernfile" | awk '{print $1}'`
             OUT "\t\t{ \"Distro\" : \"${DISTRO}\",\n\t\t \"Date\" : \"${kerndate}\",\n\t\t \"Kernel\" : \"`basename $kernfile`\"\n\t\t}"
        done
    fi
done

echo -e "\n\t]\n}" >> $outfile

if [ -f $outfile ]; then
    ssh 10.150.24.13 "mkdir -p /DailyBuilds/Daily_Builds/LinuxNewKernels/"
    scp $outfile 10.150.24.13:/DailyBuilds/Daily_Builds/LinuxNewKernels/
    rm -f $outfile
fi
