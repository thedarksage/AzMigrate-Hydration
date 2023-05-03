#include <iostream>
#include "inm_scsi_id.h"
#include "deviceidinformer.h"

int main(int argc, char *argv[])
{
        /* SetLogLevel(SV_LOG_DISABLE); */

	//printf("%s: scsi id tool \n", argv[0]);

	if (argv[1] != NULL) {
	    std::string deviceName = argv[1];
	    //printf("%s: scsi id tool device name:%s\n", argv[0], argv[1]);
        DeviceIDInformer f;
        /* TODO: now driver can pass device name instead
         * of raw for sun */
	    std::string scsi_id = f.GetID(deviceName);
	    //printf("%s: scsi id for device:%s is :%s:\n", argv[0], deviceName.c_str(),
        //       scsi_id.c_str());
	    std::cout<<scsi_id.c_str()<<"\n";
	    //printf("%s\n", scsi_id.c_str());
	    return 0;
	}
	else {
	    return 1;
	}
}

