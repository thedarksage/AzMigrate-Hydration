#ifndef _VOLUME_IMAGE_TEST_H_
#define _VOLUME_IMAGE_TEST_H_

#define MAX_COMMAND_PENDING 5
#define COMMAND_TIMEOUT 6                               //6 clock ticks ~ 93 millisec
#define COMMAND_THREAD_PERIOD 3                         //3 clock ticks ~ 46 millisec

VOID VVolumeImageProcessWriteRequestTest(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand);
VOID CommandExpiryMonitor(PVOID pContext);

#endif
