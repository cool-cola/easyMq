#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <string>
using namespace std;

void HexStr2Bin(char* pStrHex,string& strBin)
{
	char szHexItem[3];
	memset(szHexItem,0,sizeof(szHexItem));

	int iVal;
	int iHexLen = strlen(pStrHex);
	if(iHexLen%2)
	{
		memcpy(szHexItem,pStrHex,1);
		sscanf(szHexItem, "%x", &iVal);
		char cVal = iVal;
		strBin.append((char*)&cVal, sizeof(char));
		pStrHex++;
	}
	
	iHexLen = strlen(pStrHex);
	char* pPos = pStrHex;
	while((pPos -pStrHex) < iHexLen)
	{
		memcpy(szHexItem,pPos,2);

		int iVal;
		sscanf(szHexItem, "%x", &iVal);
		char cVal = iVal;

		strBin.append((char*)&cVal, sizeof(char));
		pPos += 2;
	}
}

int main(int argc, char** argv)
{
	if(argc < 4)
	{
		printf("usage: %s [shm_key hex/dec] [mirror/recover] [mirror_file]\n",argv[0]);
		printf("mirror share memory to disk or recover share memory form disk.\n\n");
		return 0;
	}
	char *pShmKey = argv[1];
	char * pOper = argv[2];
	char * pFile = argv[3];

	key_t tShmKey = 0;
	if(strstr(pShmKey,"0x") || strstr(pShmKey,"0X"))
	{
		string strBin;
		HexStr2Bin(pShmKey+2,strBin);

		int j = 0;
		for(int i=strBin.length()-1; i>=0; i--)
		{
			char *pc = (char*)&tShmKey;
			pc[j++] = strBin[i];
		}
		printf("shm_key is %d\n",(int)tShmKey);
	}
	else
	{
		tShmKey = atoi(pShmKey);
	}
	
	if(strstr(pOper,"mirror"))
	{
		int iShmID = shmget(tShmKey,0,0666);
		if( iShmID < 0)
		{
			printf("error: this share memory does not exist!\n");
			return 0;
		}

		struct shmid_ds dsbuf;
		int iRet = shmctl(iShmID,IPC_STAT,&dsbuf);
		if( iRet != 0)
		{
			printf("error: shmctl error!\n");
			return 0;
		}	

		printf("shm_segsz = %d\n",(int)dsbuf.shm_segsz);

		//if shall not be attached, shmat() shall return -1	
		char *pShmPtr = (char *)shmat( iShmID,NULL,0);
		if(pShmPtr == (char*)-1)
		{
			printf("error: shmat error!\n");
			return 0;
		}
		
		FILE *fp = fopen(pFile,"wb+");
		if(!fp)
		{
			printf("error: open %s error!\n",pFile);
			return 0;
		}		

		fwrite(pShmPtr,1,dsbuf.shm_segsz,fp);
		fclose(fp);
		printf("mirror to %s ok!\n",pFile);
		return 0;
	}
	else if(strstr(pOper,"recover"))
	{
		FILE *fp = fopen(pFile,"r");
		if(!fp)
		{
			printf("error: open %s error!\n",pFile);
			return 0;
		}		
		fseek(fp,0,SEEK_END);
		int iFileLen = ftell(fp);
		fseek(fp,0,SEEK_SET);
		
		int iShmID = shmget(tShmKey,iFileLen,IPC_CREAT|IPC_EXCL|0666);
		if(iShmID < 0)
		{
			printf("create shm error,may be exist!\n");
			return 0;
		}
		char *pShmPtr = (char *)shmat( iShmID,NULL,0);
		if(pShmPtr == (char*)-1)
		{
			printf("error: shmat error!\n");
			return 0;
		}

		fread(pShmPtr,1,iFileLen,fp);
		fclose(fp);
	
		printf("recover from %s ok,%d bytes!\n",pFile,iFileLen);
	}
	else
	{
		printf("usage: %s [shm_key hex/dec] [mirror/recover] [mirror_file]\n",argv[0]);
		printf("mirror share memory to disk or recover share memory form disk.\n\n");
		return 0;	
	}
	return 0;
}


