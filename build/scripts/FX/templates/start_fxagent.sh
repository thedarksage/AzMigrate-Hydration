#! /bin/sh

echo "--------------------------------------------"
echo `date`

# Check whether installation directory is mounted or not. If it is not mounted wait for 60 secs.
# Repeat the loop for 30 times and exit if installation directory is not mounted even after 30 mins.
# If installation directory is mounted within 30 mins then invoke soft_removal.sh.
i="1"
while [ $i -lt 31 ]
do
        echo "Iteration : $i"
        if [ -f RUNTIME_INSTALL_PATH/start ]; then
                echo "RUNTIME_INSTALL_PATH/start is available"
                sleep 15
                echo "Inovking RUNTIME_INSTALL_PATH/start"
                RUNTIME_INSTALL_PATH/start
                break
        else
                if [ "$i" = "30" ]; then
                        echo "Installation directory is not mounted even after 30 minutes. Existing..."
                        exit
                fi
                echo "Installation directory is not mounted. sleeping for 60 secs"
                sleep 60
        fi
        i=`expr $i + 1`
done
