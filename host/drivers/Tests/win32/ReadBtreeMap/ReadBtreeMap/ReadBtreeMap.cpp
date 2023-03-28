// ReadBtreeMap.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


int _tmain(int argc, _TCHAR* argv[])
{
    if(argc < 2) {
        printf("usage ReadBtreeMap.exe <btree-map-file-name>\n");
        return 0;
    }

    DiskBtree   diskBtree;
    if(!diskBtree.InitFromDiskFileName(argv[1], 0, FILE_SHARE_READ))
        return 0;
    diskBtree.DisplayHeader();
    diskBtree.BtreePreOrderTraverse();
	return 1;
}

