///
/// \file inmmagic.h
///
/// \brief inmage magic bytes used to identify file contents
/// 

#ifndef INMMAGIC__H
#define INMMAGIC__H

#define INM_BYTEMASK(ch)    ((ch) & (0xFF))
#define INM_MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
    (INM_BYTEMASK(ch0) | (INM_BYTEMASK(ch1) << 8) |   \
    (INM_BYTEMASK(ch2) << 16) | (INM_BYTEMASK(ch3) << 24 ))
	
#define INM_MAGIC_01                  INM_MAKEFOURCC( 'I', 'N', '0', '1' )
#define INM_MAGIC_02                  INM_MAKEFOURCC( 'I', 'N', '0', '2' )
#define INM_MAGIC_03                  INM_MAKEFOURCC( 'I', 'N', '0', '3' )
#define INM_MAGIC_04                  INM_MAKEFOURCC( 'I', 'N', '0', '4' )

#define INM_MAGIC_FILEMGR_IMPL_NOOP  INM_MAGIC_01
#define INM_MAGIC_BTREE              INM_MAGIC_02

#endif