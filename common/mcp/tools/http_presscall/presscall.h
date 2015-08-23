#ifndef _PRESSCALL_H_
#define _PRESSCALL_H_
/*
 * Copyright (c) 2005 by zhongchaoyu
 * Copyright 2005, 2005 by zhongchaoyu.  All rights reserved.
 */
#include "tlib_cfg.h"

#define CFGFILE "./conf.cfg"

//呼叫结果元素
struct  TUsrResult
{
	//总呼量
	int iAllReqNum;
	int iOkResponseNum;
	int iNoResponseNum;
	int iBadResponseNum;
};

struct  TResult
{
	int iAllReqNum;
	int iOkResponseNum;
	int iNoResponseNum;
	int iBadResponseNum;
	unsigned long long ullRspTimeUs;
	
	unsigned long long ullMaxRspTimeUs;
	int m_iTimeL1Num;
	int m_iTimeL2Num;
	int m_iTimeL3Num;
};

#endif

