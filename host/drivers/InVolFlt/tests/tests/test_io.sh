doio()
{
    local host=""
    local offset=0
    local i=1
    local blksz=4096
    msz=1048576

    while [ 1 -eq 1 ]
    do
        write_pattern $@ -s $blksz -i -l -o $offset -p $i

        i=`expr $i + 1`
    
        offset=`expr $offset + $blksz`
        offset=`expr $offset % $msz`
        
        if [ $offset -eq 0 ]
        then
            log "Wrapped..."
        fi

    done
}

doio "-d $1"
