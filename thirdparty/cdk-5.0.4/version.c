#include <cdk.h>

/*
 * $Author: vegogine $
 * $Date: 2013/12/24 18:06:02 $
 * $Revision: 1.1 $
 */
const char *CDKVersion (void)
{
   return CDK_VERSION_MAJOR "." CDK_VERSION_MINOR " - " CDK_VERSION_PATCH;
}
