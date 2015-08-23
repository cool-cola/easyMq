// file: .../c++-lib/inc/asn-incl.h - includes all of the asn1 library files
//
// Copyright (C) 1992 Michael Sample and the University of British Columbia
//
// This library is free software; you can redistribute it and/or
// modify it provided that this copyright/license information is retained
// in original form.
//
// If you modify this file, you must clearly indicate your changes.
//
// This source code is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// $Header: /data/cvs/prototype/story/src/public/tools/snacc-1.3/c++-lib/inc/asn-incl.h,v 1.1 2006/10/16 09:03:11 joywu Exp $
// $Log: asn-incl.h,v $
// Revision 1.1  2006/10/16 09:03:11  joywu
// no message
//
// Revision 1.5  1997/02/16 20:25:37  rj
// check-in of a few cosmetic changes
//
// Revision 1.4  1995/07/24  17:52:33  rj
// changed `_' to `-' in file names.
//
// Revision 1.3  1994/10/08  04:18:01  rj
// code for meta structures added (provides information about the generated code itself).
//
// code for Tcl interface added (makes use of the above mentioned meta code).
//
// virtual inline functions (the destructor, the Clone() function, BEnc(), BDec() and Print()) moved from inc/*.h to src/*.C because g++ turns every one of them into a static non-inline function in every file where the .h file gets included.
//
// made Print() const (and some other, mainly comparison functions).
//
// several `unsigned long int' turned into `size_t'.
//
// Revision 1.2  1994/08/28  10:00:49  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:33  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#ifndef _ASN_INCL_H_
#define _ASN_INCL_H_

#ifdef _IBM_ENC_
#define ChoiceUnion
#endif /* _IBM_ENC_ */

#include "asn-config.h"
#include "asn-buf.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-type.h"
#include "asn-int.h"
#include "asn-bool.h"
#include "asn-real.h"
#include "asn-oid.h"

#include "asn-list.h"

#include "asn-octs.h"
#include "asn-bits.h"
#include "asn-enum.h"
//#include "asn-null.h"
//#include "asn-any.h"
//#include "asn-useful.h"
#include "print.h"


/*
pData,unDataLen:包数据

iPkgTheoryLen: 包的理论长度,在只收到部分数据的情况下就可以知道
				包长度了，0为无法判断
				
return :
	>0 real msg len,
	=0 not complete,
	<0 error,must close link
*/
/*
static int asn20_complete_func(const void* pData, unsigned int unDataLen,int &iPkgTheoryLen)
{
	iPkgTheoryLen = 0;
	if (unDataLen <= sizeof(int)*2)	return 0;

	if (ntohl(*(int*)pData) != 0x4E534153)
	{
		return -1;
	}

	iPkgTheoryLen = ntohl(*((int*)pData+1));
	if (iPkgTheoryLen <= (int)unDataLen)
	{
		return iPkgTheoryLen;
	}

	return 0;
}
*/
/*
pData,unDataLen:包数据

iPkgTheoryLen: 包的理论长度,在只收到部分数据的情况下就可以知道
				包长度了，0为无法判断
				
return :
	>0 real msg len,
	=0 not complete,
	<0 error,must close link
*/
#define asn20_complete_func(pData,unDataLen,iPkgTheoryLen) \
({	\
	int iRet = 0;\
	do{\
		iPkgTheoryLen = 0;\
		if ((int)unDataLen <= (int)sizeof(int)*2) break;\
		if (ntohl(*(int*)pData) != 0x4E534153)\
		{\
			iRet = -1;break;\
		}\
		iPkgTheoryLen = ntohl(*((int*)pData+1));\
		if ((int)iPkgTheoryLen <= (int)unDataLen)\
		{\
			iRet = iPkgTheoryLen;\
		}\
	}while(0);\
iRet;})

/*
能够判断出理论包长所需要的最少数据量。
*/
/*
static int asn20_header_len()
{
	//需要8个字节就可以知道iPkgTheoryLen
	return sizeof(int)*2;
}
*/

#define asn20_header_len() ({sizeof(int)*2;})

#endif

