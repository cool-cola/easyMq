/************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2010
Description: define the manage message struct
***********************************************************/

#ifndef __CCS_ADMIN_H__
#define __CCS_ADMIN_H__

#define ADMIN_VAL_NUM	64

typedef struct
{
	int m_iAdminCmd;
	enum
	{
		CMD_FETCH_LOAD = 1,
	};

	
	int m_iVal[ADMIN_VAL_NUM];
	void Decode()
	{
		m_iAdminCmd = ntohl(m_iAdminCmd);
		for (int i=0; i<ADMIN_VAL_NUM;i++)
			m_iVal[i] = ntohl(m_iVal[i]);
	}
	void Encode()
	{
		m_iAdminCmd = htonl(m_iAdminCmd);
		for (int i=0; i<ADMIN_VAL_NUM;i++)
			m_iVal[i] = htonl(m_iVal[i]);
	}	
}TAdminMsg;

#endif

