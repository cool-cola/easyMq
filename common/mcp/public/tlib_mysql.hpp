/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2006-09
Description: mysql封装

***********************************************************/

/*
STMT 普遍的速度提高在30%左右
STMT的语句不使用查询Cache
*/

#ifndef _CMYSQL_DB_H_
#define _CMYSQL_DB_H_ 

//g++ -I/usr/local/mysql/include/ tlib_mysql.cpp -L/usr/local/mysql/lib/ -lmysqlclient -lz -lpthread

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "mysql.h"

class CMysqlConn
{
public:
	CMysqlConn();
	~CMysqlConn();

	//0 success
	int Connect(const char *szHostAddress, const char *szUserName, const char *szPassword,
			const char *szDBName, unsigned short usPort = 0);
	int Close();
	void Ping();
	
	int ExecSelect(const char *pSql);
	int ExecUpdate(const char *pSql,int& iInsertID, int& iAffectRows);
	int FetchRow();

	bool IsConnect(){return m_bDBConnect;}

	int ReConnect();
	char* GetErrMsg(){return m_szErrMsg;};
	int GetErrNo(){return m_iErrNo;};

	int GetResNum(){return m_iResultNum;}
private:
	static pthread_mutex_t stDBConnMutex;        //同步连接DB的互斥变量
	
	int m_iErrNo;
	char m_szErrMsg[512];
	
	char m_szHostAddress[64];    //DB Server 的地址
	char m_szUserName[64];        //用户名
	char m_szPassword[64];        //密码
	char m_szDBName[64];        //Database 名字
	unsigned short m_usDBPort; // 数据库端口
    	bool m_bDBConnect;        //是否已经连接上
    	
    	MYSQL m_stMysql;        //当前打开的Mysql连接
	MYSQL_RES        *m_pstRes;        //当前操作的RecordSet
          
	bool m_bHaveResult;        //当前操作的RecordSet是否为空
	int m_iResultNum;        //当前操作的RecordSet的记录数目

public:
	MYSQL_ROW        m_stRow;            //当前操作的一行
};

//---------------------------------
/*
typedef struct MYSQL_BIND
{
  unsigned long	*length;          		// output length pointer 
  my_bool       *is_null;	  				// Pointer to null indicator 
  void		*buffer;	  				// buffer to get/put data 
  enum enum_field_types buffer_type;		// buffer type 
  unsigned long buffer_length;    			// buffer length, must be set for str/binary 
  ...
}
*/
class CStmtConn
{
public:
	CStmtConn();
	~CStmtConn();

	//0 success
	int Connect(char *szHostAddress, char *szUserName, char *szPassword,  char *szDBName);
	int Close();

	int Prepare(char* pParamSQL);
	int QueryBind(MYSQL_BIND* pQueryBind);
	int ResultBind(MYSQL_BIND* pResultBind);
	
	int ExecSelect();
	int ExecUpdate(int& iInsertID, int& iAffectRows);
	int Fetch();

	int ReConnect();
	char* GetErrMsg(){return m_szErrMsg;};
	int GetErrNo(){return m_iErrNo;};

	int GetResNum(){return m_iResultNum;}
private:
	static pthread_mutex_t stDBConnMutex;        //同步连接DB的互斥变量
	
	int m_iErrNo;
	char m_szErrMsg[256];
	
	char m_szHostAddress[64];    //DB Server 的地址
	char m_szUserName[64];        //用户名
	char m_szPassword[64];        //密码
	char m_szDBName[64];        //Database 名字
	
    	bool m_bDBConnect;        //是否已经连接上
    	
    	MYSQL m_stMysql;        //当前打开的Mysql连接
    	MYSQL_STMT* m_pStmt;
          
	bool m_bHaveResult;        //当前操作的RecordSet是否为空
	int m_iResultNum;        //当前操作的RecordSet的记录数目
};

#endif
