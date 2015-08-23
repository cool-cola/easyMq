#include "../comm_mysql/comm_mysql.hpp"
/*
mysql_fetch_lengths
mysql_data_seek
*/
pthread_mutex_t CMysqlConn::stDBConnMutex = PTHREAD_MUTEX_INITIALIZER;

CMysqlConn::CMysqlConn()
{
	m_iErrNo = 0;
	m_szErrMsg[0] = 0;

	m_bDBConnect = false;

	mysql_init(&m_stMysql);
	m_pstRes = NULL;
	m_bHaveResult = false;
	m_iResultNum = 0;	

	memset(m_szHostAddress,0,sizeof(m_szHostAddress));
}

CMysqlConn::~CMysqlConn()
{
	Close();
}

int CMysqlConn::Connect(const char *szHostAddress, const char *szUserName,
	const char *szPassword, const char *szDBName, const char *szCharSet, unsigned short usPort)
{
#if MYSQL_VERSION_ID > 50012
	mysql_options(&m_stMysql, MYSQL_OPT_RECONNECT, "1");
#endif

	//���к�������mysql_real_connect()Ŀǰ���̰߳�ȫ��
	pthread_mutex_lock(&stDBConnMutex);
	if (mysql_real_connect(&m_stMysql, 
			szHostAddress, szUserName, szPassword,szDBName,usPort,NULL,CLIENT_MULTI_STATEMENTS) == 0)
	{
		pthread_mutex_unlock(&stDBConnMutex);
		snprintf(m_szErrMsg,sizeof(m_szErrMsg), "Fail To Connect To Mysql: %s", mysql_error(&m_stMysql));
		return -1;
	}   
	pthread_mutex_unlock(&stDBConnMutex);

	m_bDBConnect = true;
	
    if (NULL != szCharSet)
    {
        if (mysql_set_character_set(&m_stMysql,szCharSet) < 0)
        {
            snprintf(m_szErrMsg,sizeof(m_szErrMsg), "Cannot Set Character_set : %s", mysql_error(&m_stMysql));
            return -1;
        }
        strcpy(m_szCharSet, szCharSet);
    }

	if (strcmp(szDBName,"") != 0)
	{
	    if (mysql_select_db(&m_stMysql, szDBName) < 0)
	    {
	        snprintf(m_szErrMsg,sizeof(m_szErrMsg), "Cannot Select Database %s: %s", szDBName, mysql_error(&m_stMysql));
	        return -1;
	    }
	}	

	strcpy(m_szHostAddress, szHostAddress);
	strcpy(m_szUserName, szUserName);    
	strcpy(m_szPassword, szPassword);    
	strcpy(m_szDBName, szDBName);
	m_usDBPort = usPort;
	strcpy(m_szErrMsg, "");
	return 0;	
}

void CMysqlConn::Ping()
{
	//��鵽�������������Ƿ�����������Ͽ�,���Զ��������ӡ�
	mysql_ping(&m_stMysql);
}

int CMysqlConn::Close()
{
	if (m_bHaveResult)
	{
		mysql_free_result(m_pstRes);
		m_bHaveResult = false;
		m_iResultNum = 0;
	}    

	if(m_bDBConnect)
	{
		mysql_close(&m_stMysql);
		m_bDBConnect = false;
	}
	return 0;
}

int CMysqlConn::ExecSelect(const char *pSql) 
{
	if (m_bHaveResult)
	{
		mysql_free_result(m_pstRes);
		m_bHaveResult = false;
		m_iResultNum = 0;
	}        

	if (!m_bDBConnect)
	{
		ReConnect();
		if (!m_bDBConnect)
		{
			strcpy(m_szErrMsg, "Has Not Connect To DB Server Yet");
			return -1;		
		}
	}

	// ִ����Ӧ��SQL���
	int iRetCode =mysql_query(&m_stMysql,pSql);
	if (iRetCode != 0)
	{
		ReConnect();
		iRetCode =mysql_query(&m_stMysql,pSql);
		if(iRetCode != 0)
		{
			//������Ϣ
			m_iErrNo=mysql_errno(&m_stMysql);
			snprintf(m_szErrMsg,sizeof(m_szErrMsg),"Fail To Execute SQL: %s:%s", pSql,mysql_error(&m_stMysql));		
			return -1;
		}
	}
    
	// ������
	m_pstRes = mysql_store_result(&m_stMysql);
	if(m_pstRes)
	{
		m_bHaveResult = true;	
		m_iResultNum = mysql_num_rows(m_pstRes);
	}
	/* mysql�ֲ�:
	�п�����һ����mysql_query()�ɹ��ĵ��ú�mysql_store_result()����NULL��
	���ⷢ��ʱ������ζ�ų�������������֮һ:

	��һ��malloc()ʧ��(���磬����������̫��)�� 
	���ݲ��ܱ���ȡ(�����������ϵ�һ������)�� 
	��ѯû�з�������(���磬����һ��INSERT��UPDATE��DELETE)��	
	*/
	return 0;
}

int CMysqlConn::ExecUpdate(const char *pSql,int& iInsertID, int& iAffectRows) 
{ 
	if (m_bHaveResult)
	{
		mysql_free_result(m_pstRes);
		m_bHaveResult = false;
		m_iResultNum = 0;
	}   
	
	if (!m_bDBConnect)
	{
		ReConnect();
		if (!m_bDBConnect)
		{
			strcpy(m_szErrMsg, "Has Not Connect To DB Server Yet");
			return -1;
		}
	}

	// ִ����Ӧ��SQL���
	int iRetCode =mysql_query(&m_stMysql, pSql);
	if (iRetCode != 0)
	{
		ReConnect();
		iRetCode =mysql_query(&m_stMysql, pSql);
		if(iRetCode != 0)
		{
			//������Ϣ
			m_iErrNo=mysql_errno(&m_stMysql);
			snprintf(m_szErrMsg,sizeof(m_szErrMsg), "Fail To Execute SQL: %s:%s",
						pSql,mysql_error(&m_stMysql));		
			return -3;
		}
	}
    
	// ������
	iInsertID = mysql_insert_id(&m_stMysql);
	iAffectRows = mysql_affected_rows(&m_stMysql);
	return 0;
}

int CMysqlConn::FetchRow()
{
    if (!m_bHaveResult)
    {
        sprintf(m_szErrMsg, "Recordset is Null");
        return -1;    
    }        
    
    if (m_iResultNum == 0)
    {
        sprintf( m_szErrMsg, "Recordset count=0");
        return -1;    
    }        

    m_stRow = mysql_fetch_row(m_pstRes);
    return 0;
}

int CMysqlConn::ReConnect()
{
	Close();

	if(m_szHostAddress[0] == 0)
		return -1;
	
	return Connect(m_szHostAddress,m_szUserName,m_szPassword,m_szDBName,m_szCharSet,m_usDBPort);
}

//------------------------------------------------------------
pthread_mutex_t CStmtConn::stDBConnMutex = PTHREAD_MUTEX_INITIALIZER;

CStmtConn::CStmtConn()
{
	m_iErrNo = 0;
	m_szErrMsg[0] = 0;

	m_bDBConnect = false;

	mysql_init(&m_stMysql);
	m_pStmt = NULL;
	
	m_bHaveResult = false;
	m_iResultNum = 0;	
}

CStmtConn::~CStmtConn()
{
	Close();
}

int CStmtConn::Connect(char *szHostAddress, char *szUserName, char *szPassword, 
					char *szDBName)
{
#if MYSQL_VERSION_ID > 50012
	mysql_options(&m_stMysql, MYSQL_OPT_RECONNECT, "1");
#endif

	//���к�������mysql_real_connect()Ŀǰ���̰߳�ȫ��
	pthread_mutex_lock(&stDBConnMutex);
	if (mysql_real_connect(&m_stMysql, 
			szHostAddress, szUserName, szPassword,szDBName,0,NULL,0) == 0)
	{
		pthread_mutex_unlock(&stDBConnMutex);
		sprintf(m_szErrMsg, "Fail To Connect To Mysql: %s", mysql_error(&m_stMysql));
		return -1;
	}   
	pthread_mutex_unlock(&stDBConnMutex);

	m_bDBConnect = true;
	
	if (strcmp(szDBName,"") != 0)
	{
	    if (mysql_select_db(&m_stMysql, szDBName) < 0)
	    {
	        sprintf(m_szErrMsg, "Cannot Select Database %s: %s", szDBName, mysql_error(&m_stMysql));
	        return -2;
	    }
	}	

	m_pStmt = mysql_stmt_init(&m_stMysql);
	if(!m_pStmt)
	{
	        sprintf(m_szErrMsg, "mysql_stmt_init failed!");
	        return -3;	
	}
	
	strcpy(m_szHostAddress, szHostAddress);
	strcpy(m_szUserName, szUserName);    
	strcpy(m_szPassword, szPassword);    
	strcpy(m_szDBName, szDBName);    
	
	strcpy(m_szErrMsg, "");
	return 0;	
}

int CStmtConn::Close()
{
	if (m_bHaveResult)
	{
		mysql_stmt_free_result(m_pStmt);
		m_bHaveResult = false;
		m_iResultNum = 0;
	}    

	if(m_pStmt)
	{
		mysql_stmt_close(m_pStmt);
		m_pStmt = NULL;
	}
	
	if(m_bDBConnect)
	{
		mysql_close(&m_stMysql);
		m_bDBConnect = false;
	}

	return 0;
}

int CStmtConn::Prepare(char* pParamSQL)
{
	return  mysql_stmt_prepare(m_pStmt, pParamSQL, strlen(pParamSQL));
}

int CStmtConn::QueryBind(MYSQL_BIND* pQueryBind)
{
	return  mysql_stmt_bind_param(m_pStmt, pQueryBind);
}

int CStmtConn::ResultBind(MYSQL_BIND* pResultBind)
{
	return  mysql_stmt_bind_result(m_pStmt, pResultBind);
}

int CStmtConn::ExecSelect() 
{
	if (m_bHaveResult)
	{
		mysql_stmt_free_result(m_pStmt);
		m_bHaveResult = false;
		m_iResultNum = 0;
	}        

	if (!m_bDBConnect)
	{
		strcpy(m_szErrMsg, "Has Not Connect To DB Server Yet");
		return -1;
	}
	
	// ִ����Ӧ��SQL���
	int iRetCode = mysql_stmt_execute(m_pStmt);
	if (iRetCode != 0)
	{
		ReConnect();
		iRetCode = mysql_stmt_execute(m_pStmt);
		if(iRetCode != 0)
		{
			//������Ϣ
			m_iErrNo=mysql_stmt_errno(m_pStmt);
			snprintf(m_szErrMsg,sizeof(m_szErrMsg), "Fail To Execute STMT SQL: %s", mysql_stmt_error(m_pStmt));		
			return -1;
		}
	}
    
	// ������
	m_bHaveResult = true;	
	mysql_stmt_store_result(m_pStmt);
	m_iResultNum = mysql_stmt_num_rows(m_pStmt);
	return 0;
}

int CStmtConn::ExecUpdate(int& iInsertID, int& iAffectRows) 
{ 
	if (m_bHaveResult)
	{
		mysql_stmt_free_result(m_pStmt);
		m_bHaveResult = false;
		m_iResultNum = 0;
	}        

	if (!m_bDBConnect)
	{
		strcpy(m_szErrMsg, "Has Not Connect To DB Server Yet");
		return -1;
	}
	
	// ִ����Ӧ��SQL���
	int iRetCode = mysql_stmt_execute(m_pStmt);
	if (iRetCode != 0)
	{
		ReConnect();
		iRetCode = mysql_stmt_execute(m_pStmt);
		if(iRetCode != 0)
		{
			//������Ϣ
			m_iErrNo=mysql_stmt_errno(m_pStmt);
			snprintf(m_szErrMsg,sizeof(m_szErrMsg), "Fail To Execute STMT SQL: %s", mysql_stmt_error(m_pStmt));		
			return -1;
		}
	}
    
	// ������
	iInsertID = mysql_stmt_insert_id(m_pStmt);
	iAffectRows = mysql_stmt_affected_rows(m_pStmt);
	return 0;
}

// 0 is success
int CStmtConn::Fetch()
{
    if (!m_bHaveResult)
    {
        sprintf(m_szErrMsg, "Recordset is Null");
        return -1;    
    }        
    
    if (m_iResultNum == 0)
    {
        sprintf( m_szErrMsg, "Recordset count=0");
        return -1;    
    }        

    return mysql_stmt_fetch(m_pStmt);;
}

int CStmtConn::ReConnect()
{
	Close();
	return Connect(m_szHostAddress,m_szUserName,m_szPassword,m_szDBName);
}

#if 0
char dbnames[][64]=
{
"taotao_uinfo_",
"taotao_uindex_",
"taotao_ft_",
"taotao_qft_",
"taotao_findex_",
"taotao_qfindex_"
};

char tabnames[][64]=
{
"t_taotao_uinfo_",
"t_taotao_uindex_",
"t_taotao_ft_",
"t_taotao_qft_",
"t_taotao_findex_",
"t_taotao_qfindex_"
};

#include <stdlib.h>
int main()
{
	CMysqlConn stMysqlConn;
	int cnt = 0;
	time_t ts = time(0);
	
	if(stMysqlConn.Connect("localhost" ,"root","","kccd_bitmap") != 0)
	{
		printf("conn failed!");
		return -1;
	}
			char sztab[1024];

	cnt = 0;
	ts = time(0);
	char szcon[128];
	memset(szcon,'s',sizeof(szcon));
	szcon[127] = 0;

	#if 1
	int id = 1;
	int tabidx = 0;
	while(1)
	{

		if (stMysqlConn.ExecSelect("select ip,port from t_server_list where role=1 and ip='10.130.28.227'"))
	    {
	        return -1;
	    }

		printf("%d rows\n",stMysqlConn.GetResNum());
		sleep(1);
		continue;
		/*
		int kk,kkk;
		if(stMysqlConn.ExecUpdate(sztab,kk,kkk))
			return 0;

			sprintf(sztab,"insert into mtest_2.test%d values(%d,'%s')",tabidx,id++,szcon);

		if(stMysqlConn.ExecUpdate(sztab,kk,kkk))
			return 0;
		
		cnt+=2;

		tabidx++;
		if(tabidx == 3000)
		{
			printf("===============\n");
		}
		tabidx = tabidx %3000;
		*/
/*
		sprintf(sztab,"select * from  mtest.test where id=%d",id);

		int kk,kkk;
		if(stMysqlConn.ExecSelect(sztab))
			return 0;

		sprintf(sztab,"select * from  mtest.test where id=%d",561625-id);
	id++;

		if(stMysqlConn.ExecSelect(sztab))
			return 0;
		
		*/
		
		
		time_t te = time(0);
		if(te-ts>=1)
		{
			ts =te;
			printf("cnt=%d\n",cnt);
			cnt = 0;
		}
		
	}	

		return 0;
#endif
	cnt = 0;
	//srand(time(0));
	
	while(1)
	{
		int dbid = rand()%100;
		int tabid = rand()%100;
		int dbidx = rand()%6;
		int tableidx = rand()%100;
		int uin=445566;//rand();
		
		dbidx = 3;

		char sztab[256];
		sprintf(sztab,"select data from %s%d.%s%d where uin=%d",
			dbnames[dbidx],dbid,tabnames[dbidx],tableidx,uin);

	//sprintf(sztab,"insert into %s%d.%s%d(uin,data) values(%d,'%s')",
			//dbnames[dbidx],dbid,tabnames[dbidx],tableidx,uin,szcon);		

		int jj,kk;
		if(stMysqlConn.ExecSelect(sztab))
		{
			//continue;
			return -1;
		}
		if(!stMysqlConn.GetResNum())
			return -1;		
		cnt++;
		
		time_t te = time(0);
		if(te-ts>=1)
		{
			ts =te;
			printf("cnt=%d\n",cnt);
			cnt = 0;
		}
	}
	/*
	for (int i=0; i<stMysqlConn.GetResNum();i++)
	{
		stMysqlConn.f
		if(stMysqlConn.FetchRow())
			break;

		if(!stMysqlConn.m_stRow)
			break;
		
		printf("%d %s\n",atoi(stMysqlConn.m_stRow[0]),stMysqlConn.m_stRow[1]);
	}
	*/
	return 0;
}

#endif
/*
main()
{
	CStmtConn stStmtConn;
	if(stStmtConn.Connect("localhost" ,"","","tdbdump") != 0)
	{
		printf("conn failed!");
		return -1;
	}

	//׼����ʽSQL
       stStmtConn.Prepare("select id1,name from tt where id1>? and id1<?");

	//��ʹ��blob����Ҳ���ʺ�,���ܼ�����
	//stStmtConn.Prepare("insert into t1(id1,name) values(?,?)");

	//ѭ����ʼ��,�ı�idmin,idmax��ֵ,����bind�ȿ�.............................................................
	
	//param bind------------------------------
       MYSQL_BIND queryBind[2];
	memset(queryBind, 0, sizeof(queryBind));

	int idmin = 50;
	int idmax = 100;	
 	queryBind[0].buffer_type = MYSQL_TYPE_LONG;
       queryBind[0].buffer_length = -1;
       queryBind[0].buffer = (char *)&idmin;

 	queryBind[1].buffer_type = MYSQL_TYPE_LONG;
       queryBind[1].buffer_length = -1;
       queryBind[1].buffer = (char *)&idmax;
  
       stStmtConn.QueryBind(queryBind);

	//result bind------------------------------
	MYSQL_BIND resBind[2];
	memset(resBind, 0, sizeof(resBind));
	
	int resid;
	char resblob[1024];
 	resBind[0].buffer_type = MYSQL_TYPE_LONG;
       resBind[0].buffer_length = -1;
       resBind[0].buffer = (char *)&resid;

 	resBind[1].buffer_type = MYSQL_TYPE_BLOB;
       resBind[1].buffer_length = sizeof(resblob);
       resBind[1].buffer = (char *)resblob;

	stStmtConn.ResultBind(resBind);

	if(stStmtConn.ExecSelect())
		return -1;
	
	for (int i=0; i<stStmtConn.GetResNum();i++)
	{
		memset(resblob,0,sizeof(resblob));
		
		if(stStmtConn.Fetch())
			break;
		
		printf("%d %s\n",resid,resblob);
	}
	return 0;
}
*/

