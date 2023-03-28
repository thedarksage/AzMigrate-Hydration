#ifndef _PARTITION_TYPES_H
#define _PARTITION_TYPES_H
/* Extended partition types */

#define EXTENDED        0x05
#define WIN98_EXTENDED  0x0f
#define LINUX_EXTENDED  0x85

struct partition_types_tag 
{
	unsigned char type;
	char *name;
};

#endif /* _PARTITION_TYPES_H */
