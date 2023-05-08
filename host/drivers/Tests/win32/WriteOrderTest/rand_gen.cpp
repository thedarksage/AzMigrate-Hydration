#include<windows.h>
#include"rand_gen.h"

LONGLONG GenRandOffset(LONGLONG max_offset) {
	int i;
	LONGLONG temp=0;
    int msb;
	int bit;
    LONGLONG prev;
	for(i=62;i>=0;i--) {
		if(max_offset&(LONGLONG_ONE<<i)) break;
	}
    msb=i;
	for(i=0;i<msb;i++) {
		bit = rand()/((RAND_MAX+1)/2);
		if(bit) temp |= LONGLONG_ONE<<i;
	}
    prev=temp;
    do {
        temp = prev;
        bit = rand()/((RAND_MAX+1)/2);
		if(bit) temp |= LONGLONG_ONE<<msb;
    }while(temp > max_offset);

	return temp;
}

ULONG GenRandSize(ULONG max_size) {
	int i;
	ULONG temp=0;
    int msb;
	int bit;
    ULONG prev;

	for(i=31;i>=0;i--) {
		if(max_size&(1<<i)) break;
	}
    msb=i;
	for(i=0;i<msb;i++) {
		bit = rand()/((RAND_MAX+1)/2);
		if(bit) temp |= 1<<i;
	}
    prev=temp;
    do {
        temp = prev;
        bit = rand()/((RAND_MAX+1)/2);
		if(bit) temp |= 1<<msb;
    }while(temp > max_size);

	return temp;
}

ULONG GenRandPattern() {
	ULONG temp=0;
    int i,bit;
    for(i=0;i<32;i++) {
        bit = rand()/((RAND_MAX+1)/2);
		if(bit) temp |= 1<<i;
    }
	return temp;
}
