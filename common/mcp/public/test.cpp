#include <stdio.h>
#include <string>
using namespace std;
//#include "NodeCache.hpp"
#include "MemCache.hpp"

void PrintBin(char *pBuffer, int iLength)
{
    int i;

    char tmpBuffer[16384];
    char strTemp[32];

    if( iLength <= 0 || iLength > 4096 || pBuffer == NULL )
    {
        return;
    }

    tmpBuffer[0] = '\0';
    for( i = 0; i < iLength; i++ )
    {
        if( !(i%16) )
        {
            sprintf(strTemp, "\n%04d>    ", i/16+1);
            strcat(tmpBuffer, strTemp);
        }
        sprintf(strTemp, "%02X ", (unsigned char)pBuffer[i]);
        strcat(tmpBuffer, strTemp);
    }

    strcat(tmpBuffer, "\n");
    printf("Print Hex:%s", tmpBuffer);
    return;
}
/*
 int Match(void* pHashNode,void* pArg)
{
	NodeCache::THashNode* pNode = (NodeCache::THashNode*)pHashNode;

	int *piArg = (int*)pArg;
	
	int iMod = piArg[0];
	int iModRes = piArg[1];

	int iUin = 0;
	memcpy(&iUin,pNode->m_szKey,sizeof(int));

	if (iUin%iMod == iModRes)
		return 0;

	return -1;
}
*/
enum
{
	op_set = 1,
	op_del,
	op_set_node,
	op_append,
	op_skip
};

int blocksel(void* pBlock,void* pSelectArg)
{
	if (*(char*)pBlock == *(char*)pSelectArg)
		return 0;
	
	return -1;
}

int main(int argc ,char** argv)
{
	if (argc < 2)
	{
		printf("%s set [uin] [data]\n",argv[0]);
		printf("%s get [uin]\n",argv[0]);
		printf("%s app [uin] [data]\n",argv[0]);
		printf("%s skip [uin] [num]\n",argv[0]);
		printf("%s log\n",argv[0]);
		printf("%s clean [uin]\n",argv[0]);
		return 0;
	}	

	char szBlock[5];

	

	int MEMSIZE = 1024*1024*30;
	char *pMem = new char[MEMSIZE];

	MemCache stMemCache;
	int len = stMemCache.AttachMem(pMem, MEMSIZE,10,emInit,sizeof(szBlock));
	printf("alloc: %d\n",len);	

		char data[2560];
		memset(data,0,sizeof(data));
			unsigned int  unDataLen = 0;
			
	stMemCache.DumpInit(1, "cache.dump", MemCache::DUMP_TYPE_MIRROR, 1,"binlog",50,5);
	stMemCache.StartUp();

	char szKey[MemCache::HASH_KEY_LEN];
	memset(szKey,0,sizeof(szKey));
	memset(szBlock,0,sizeof(szBlock));

if (0==strcmp("set",argv[1]))
	{

		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);

		int ret = stMemCache.Set(szKey,argv[3], strlen(argv[3]));
		printf("ret %d\n",ret);		
	}	
	else 	if (0==strcmp("get",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);

			unsigned int datelen;
			data[0] = 0;
			int ret = stMemCache.Get(szKey,data,sizeof(data),datelen);
			printf("ret %s\n",data);		
	}
	else 	if (0==strcmp("clean",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);
			printf("ret %d\n",stMemCache.SetNodeFlg(szKey,0));		
	}	
	else 	if (0==strcmp("app",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);

			unsigned int datelen;
			int ret = stMemCache.Append(szKey,argv[3],strlen(argv[3]));
			printf("ret %d\n",ret);		
	}	
	else 	if (0==strcmp("skip",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);

		int len = atoi(argv[3]);

			int ret = stMemCache.Skip(szKey,len);
			printf("ret %d\n",ret);		
	}	
	else 	if (0==strcmp("log",argv[1]))
	{
		CBinLog m_stBinLog;
		m_stBinLog.Init("binlog",20000000,20);	
		m_stBinLog.SetReadRecordStartTime(-1);

		ssize_t iOp = 0;	
			char szHashKey[MemCache::HASH_KEY_LEN];
			ssize_t iDataLen = 0;
			char* pData = NULL;
			ssize_t iCount = 0;
			ssize_t iLogLen = 0;

char *			m_pBuffer = new char[1024*1024*3];
			
			time_t tLogTime;
			while(0<(iLogLen = m_stBinLog.ReadRecordFromBinLog(m_pBuffer, MAX_BINLOG_ITEM_LEN,tLogTime)))
			{
				ssize_t iBuffPos = 0;
				
				memcpy(&iOp,m_pBuffer,sizeof(ssize_t));
				iBuffPos += sizeof(ssize_t);

				if (iOp == op_set)
				{		
					MemCache::THashNode stHashNode;
					memcpy(&stHashNode,m_pBuffer+iBuffPos,sizeof(MemCache::THashNode));
					iBuffPos += sizeof(MemCache::THashNode);
					
					memcpy(&iDataLen,m_pBuffer+iBuffPos,sizeof(ssize_t));
					iBuffPos += sizeof(ssize_t);
					
					pData = m_pBuffer+iBuffPos;
					
					int uin;
					memcpy(&uin,stHashNode.m_szKey,4);
					pData[iDataLen] = 0;
					printf("op_set %d,%s,%d\n",uin,pData,iDataLen);	
					
				}
				else if (iOp == op_set_node)
				{
					MemCache::THashNode stHashNode;
					memcpy(&stHashNode,m_pBuffer+iBuffPos,sizeof(MemCache::THashNode));
					iBuffPos += sizeof(MemCache::THashNode);

					int uin;
					memcpy(&uin,stHashNode.m_szKey,4);
					printf("op_set_node %d\n",uin);	
					
				}		
				else if  (iOp == op_del)
				{	
					memcpy(szHashKey,m_pBuffer+iBuffPos,MemCache::HASH_KEY_LEN);
					iBuffPos += MemCache::HASH_KEY_LEN;

				
					int uin;
					memcpy(&uin,szHashKey,4);
					printf("_Del %d\n",uin);	
					
				}		
				else if (iOp == op_append)
				{		
					memcpy(szHashKey,m_pBuffer+iBuffPos,MemCache::HASH_KEY_LEN);
					iBuffPos += MemCache::HASH_KEY_LEN;
					
					memcpy(&iDataLen,m_pBuffer+iBuffPos,sizeof(ssize_t));
					iBuffPos += sizeof(ssize_t);
					
					pData = m_pBuffer+iBuffPos;

					
					int uin;
					memcpy(&uin,szHashKey,4);
					pData[iDataLen] = 0;
					printf("_Append %d,%s,%d\n,",uin,pData,iDataLen);					
				}
				else if (iOp == op_skip)
				{		
					memcpy(szHashKey,m_pBuffer+iBuffPos,MemCache::HASH_KEY_LEN);
					iBuffPos += MemCache::HASH_KEY_LEN;
					
					ssize_t iSkipLen = 0;
					memcpy(&iSkipLen,m_pBuffer+iBuffPos,sizeof(ssize_t));
					iBuffPos += sizeof(ssize_t);

					int uin;
					memcpy(&uin,szHashKey,4);
					printf("skip %d,%d\n",uin,iSkipLen);
				}
				
				iCount++;
			}
			
	}

	stMemCache.CoreDump();
/*	
	if (argc < 2)
	{
		printf("%s insert [uin] [pos] [data]\n",argv[0]);
		printf("%s delnode [uin] [pos]\n",argv[0]);
		printf("%s del [uin]\n",argv[0]);
		printf("%s app [uin] [data]\n",argv[0]);
		printf("%s trim [uin] [num]\n",argv[0]);
		printf("%s show [uin]\n",argv[0]);
		printf("%s sel [uin] [start] [num]\n",argv[0]);
		printf("%s find [uin]\n",argv[0]);
		return 0;
	}
	NodeCache stNodeCache;
	int len = stNodeCache.AttachMem(pMem, MEMSIZE,10,emInit,sizeof(szBlock));
	printf("alloc: %d\n",len);
		char data[2560];
		memset(data,0,sizeof(data));
			unsigned int  unDataLen = 0;

	stNodeCache.DumpInit(1,"cache.dump",NodeCache::DUMP_TYPE_NODE,1);
	stNodeCache.StartUp();
	
	char szKey[NodeCache::HASH_KEY_LEN];
	memset(szKey,0,sizeof(szKey));
	memset(szBlock,0,sizeof(szBlock));
	
	



if (0==strcmp("insert",argv[1]))
	{

		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);

		int pos = atoi(argv[3]);

		strcpy(szBlock,argv[4]);

			int ret = stNodeCache.InsertBlock(szKey,szBlock, pos);
			printf("ret %d\n",ret);		
	}	
	else 	if (0==strcmp("delnode",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);
		
		int pos = atoi(argv[3]);
		
			int ret = stNodeCache.DeleteBlock(szKey,pos);
			printf("ret %d\n",ret);		
	}
	else 	if (0==strcmp("find",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);
		char a = 'a';
		int pos = stNodeCache.FindBlock(szKey,blocksel, (void *)&a);
		printf("pos %d\n",pos);
	}		
	else 	if (0==strcmp("sel",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);

		int s = atoi(argv[3]);
		int n = atoi(argv[4]);

		char a = 'a';
		
		int sellen = stNodeCache.SelectBlock(szKey,data,sizeof(data),blocksel, (void *)&a, s, n);
		if (sellen>0)
		PrintBin(data,sellen);
	}	
	else 	if (0==strcmp("trim",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);
		
		int nodenum = atoi(argv[3]);
		
			int ret = stNodeCache.TrimTail(szKey,nodenum);
			printf("ret %d\n",ret);		
	}	
	else 	if (0==strcmp("del",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);

			int ret = stNodeCache.Del(szKey);
			printf("ret %d\n",ret);		
	}
	else 	if (0==strcmp("dels",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);
		char a = 'a';
			int ret = stNodeCache.DeleteBlock(szKey,blocksel,&a);
			printf("ret %d\n",ret);		
	}	
	else 	if (0==strcmp("app",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);
strcpy(szBlock,argv[3]);

			int ret = stNodeCache.AppendBlock(szKey,szBlock);
			printf("ret %d\n",ret);		
	}	
	else 	if (0==strcmp("show",argv[1]))
	{

		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);

		int num = stNodeCache.GetBlockNum(szKey);
		for (int i=0; i<num; i++)
		{
			char *pBlock = stNodeCache.GetBlockData(szKey,i);
			printf("%s\n",pBlock);
		}
		
	}
//stNodeCache.Print(stdout);
//stNodeCache.CoreDump();

FILE* fp = fopen("cache.dump","r+");
char szCache[102400];
int ilen = fread(szCache,1,102400,fp);
PrintBin(szCache,ilen);
	return 0;




*/










/*

	
	int iDataLen = 9*1024*1024;
	char *pData = new char[iDataLen];
	char *pData2 = new char[iDataLen];
	for (int i=0; i<iDataLen;i++)
	{
		pData[i] = i%255;
	}
	for (int islen=1;islen<9*1024*1024; islen++)
	{
		int sk = 100;
		//stNodeCache.Skip(szKey,sk);
		//pData+=sk;
		//iDataLen-=sk;

		unsigned int igetlen = 0;
		int ret = stNodeCache.Get(szKey,pData2, 9*1024*1024,igetlen);
		if (ret)
		{
			printf("get failed\n");
			break;
		}
		if(igetlen != iDataLen)
		{
			printf("get failed getlen %d,but must %d\n",igetlen,islen);
			break;
		}	

		if(0 != memcmp(pData,pData2,igetlen))
		{
			printf("get failed data not match islen=%d\n",islen);
			break;	
		}

		//if (islen %1000==0)
			printf("set len=%d\n",islen);
	}

		return 0;
			
	//stNodeCache.DumpInit(1,"cache.dump",NodeCache::DUMP_TYPE_NODE,1);
	//stNodeCache.StartUp();
	int iArg[2];
	iArg[0] = 10;
	iArg[1] = 1;
	
	char * pBuffer = new char[1024*100];
	FILE* fp = fopen("cache.dump","r+");
	int ilen = fread(pBuffer,1,1024,fp);
	stNodeCache.CoreRecoverMem(pBuffer, ilen);
	//stNodeCache.CleanNode(Match, (void *)iArg);
	


	if (0==strcmp("get",argv[1]))
	{

		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);

			int ret = stNodeCache.Get(szKey,data, sizeof(data), unDataLen);
			printf("ret %d,%d : %s len %d\n",ret,uin,data,unDataLen);		
	}	
	else 	if (0==strcmp("del",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);

			int ret = stNodeCache.Del(szKey);
			printf("ret %d\n",ret);		
	}	
	else 	if (0==strcmp("clean",argv[1]))
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);

			int ret = stNodeCache.MarkClean(szKey);
			printf("ret %d\n",ret);		
	}
	else
	{
		int uin = atoi(argv[2]);
		memcpy(szKey,&uin,4);
		stNodeCache.Set(szKey,argv[3], strlen(argv[3]));
	}




	int iLen = stNodeCache.CoreDumpMem(pBuffer, 1024*100, Match, (void *)iArg);
	printf("dump %d bytes\n",iLen);
	PrintBin(pBuffer,iLen);



stNodeCache.CoreDump();
*/
	return 0;
	
}
