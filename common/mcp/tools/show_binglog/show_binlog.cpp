#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <vector>
using namespace std;
#include "BinlogNR.hpp"

#define SHOWUSAGE \
{\
printf("%s show [binlog_file]\n",argv[0]);\
printf("%s info [binlog_file]\n",argv[0]);\
printf("%s cut [binlog_file] [start_time] [end_time]\n",argv[0]);\
printf("%s cutdir [binlog_dir] [end_time]\n",argv[0]);\
printf("%s dirinfo [binlog_dir]\n",argv[0]);\
printf("\nexample:\n%s cutdir ./cache0/ 1297533603\n",argv[0]);\
}

void GetCurDateTimeStr(time_t tNow,char* szTimeStr)
{
	struct tm curr;
	curr = *localtime(&tNow);

	if (curr.tm_year > 50)
	{
		sprintf(szTimeStr, "%04d-%02d-%02d %02d:%02d:%02d", 
			curr.tm_year+1900, curr.tm_mon+1, curr.tm_mday,
			curr.tm_hour, curr.tm_min, curr.tm_sec);
	}
	else
	{
		sprintf(szTimeStr, "%04d-%02d-%02d %02d:%02d:%02d",
	        curr.tm_year+2000, curr.tm_mon+1, curr.tm_mday,
	        curr.tm_hour, curr.tm_min, curr.tm_sec);
	}
}
void PrintHexStr(char *pBuffer, int iLength)
{
    if( iLength <= 0 || pBuffer == NULL )
        return;

    for(int i = 0; i < iLength; i++ )
    {
		printf("%02x", (unsigned char)pBuffer[i]);
    }
}
int main(int argc, char* argv[])
{
	if(argc < 2)    
	{        
		SHOWUSAGE	 
		return 0;    
	}

	char *pAct = argv[1];
	
	char *pLogBuff = new char[10*1024*1024];

	if(0 == strcmp(pAct,"show"))
	{
		char *pInFile = argv[2];
		FILE *m_pReadFp = fopen(pInFile,"r");
		if(!m_pReadFp)
		{
			printf("can not open %s\n",pInFile);
			return 0;
		}	
		while(!feof(m_pReadFp))
		{
			int32_t iLogSize = 0;
			int64_t tLogTime;
			char szDateTime[128];

			//只有read一次才知道feof...
			fread(&tLogTime,1,sizeof(u_int64_t),m_pReadFp);
			fread(&iLogSize,1,sizeof(int32_t),m_pReadFp);
			if (feof(m_pReadFp))
			{
				return 0;
			}
			
			int iRet = fread(pLogBuff,1,iLogSize,m_pReadFp);
			if(iRet != iLogSize)
			{
				printf("read binlog failed!\n");
				break;
			}

			GetCurDateTimeStr((time_t)tLogTime,szDateTime);

			printf("[%s][%d] ",szDateTime,iLogSize);
			PrintHexStr(pLogBuff,iLogSize);
			printf("\n");	
		}
		fclose(m_pReadFp);	
	}
	else if(0 == strcmp(pAct,"cut"))
	{
		char *pInFile = argv[2];
		FILE *m_pReadFp = fopen(pInFile,"r");
		if(!m_pReadFp)
		{
			printf("can not open %s\n",pInFile);
			return 0;
		}	
		int64_t iStartTime = atoll(argv[3]);
		int64_t iEndTime = atoll(argv[4]);

		char szOutFile[512];
		sprintf(szOutFile,"%s.cut.%lld_%lld",pInFile,(long long)iStartTime,(long long)iEndTime);
		
		CBinLog stOutBinLog;
		stOutBinLog.Init(szOutFile,500000000);

		int iCnt = 0;
		while(!feof(m_pReadFp))
		{
			int32_t iLogSize = 0;
			int64_t tLogTime;

			//只有read一次才知道feof...
			fread(&tLogTime,1,sizeof(u_int64_t),m_pReadFp);
			fread(&iLogSize,1,sizeof(int32_t),m_pReadFp);
			if (feof(m_pReadFp))
			{
				break;
			}
			
			int iRet = fread(pLogBuff,1,iLogSize,m_pReadFp);
			if(iRet != iLogSize)
			{
				printf("read binlog failed!\n");
				break;
			}

			if((tLogTime>=iStartTime) && (tLogTime<=iEndTime))
			{
				stOutBinLog.WriteToBinLog(pLogBuff,iLogSize,tLogTime);
				iCnt++;
			}	
		}
		printf("%d records write out.\n",iCnt);
	}
	else if(0 == strcmp(pAct,"info"))
	{
		char *pInFile = argv[2];
		FILE *m_pReadFp = fopen(pInFile,"r");
		if(!m_pReadFp)
		{
			printf("can not open %s\n",pInFile);
			return 0;
		}	
		int iCnt = 0;
		int64_t tFirstLogTime = 0;
		int64_t tLastLogTime = 0;
		while(!feof(m_pReadFp))
		{
			int32_t iLogSize = 0;
			int64_t tLogTime;

			//只有read一次才知道feof...
			fread(&tLogTime,1,sizeof(u_int64_t),m_pReadFp);
			fread(&iLogSize,1,sizeof(int32_t),m_pReadFp);
			if (feof(m_pReadFp))
			{
				break;
			}
			
			int iRet = fread(pLogBuff,1,iLogSize,m_pReadFp);
			if(iRet != iLogSize)
			{
				printf("read binlog failed! ret=%d\n",iRet);
				break;
			}

			if(iCnt == 0)
			{
				tFirstLogTime = tLogTime;
			}
			tLastLogTime = tLogTime;
			iCnt++;
		}

		char szFirstDateTime[128];
		char szLastDateTime[128];
		GetCurDateTimeStr((time_t)tFirstLogTime,szFirstDateTime);
		GetCurDateTimeStr((time_t)tLastLogTime,szLastDateTime);
		printf("Start Time:[%lld] %s, End Time:[%lld] %s\n",(long long)tFirstLogTime,szFirstDateTime,
						(long long)tLastLogTime,szLastDateTime);
		printf("%d records.\n",iCnt);
	}
	else if(0 == strcmp(pAct,"dirinfo"))
	{
		char *pDir = argv[2];

		//dir 句柄
		DIR *dp; 
		//dir 结构
		struct dirent *pdir;
		if ((dp = opendir(pDir)) == NULL)
		{
			printf("opendir %s error!\n",pDir);
			return -1;
		}

		int64_t tAllFirstLogTime = 2147483647;
		int64_t tAllLastLogTime = 0;
		int64_t tAllItems = 0;
		vector<string> vec_mod_file;
	    while ((pdir = readdir(dp)) != NULL)
	    {
			if(strstr(pdir->d_name,"binlog_a") || strstr(pdir->d_name,"binlog_b"))
			{
				char szFile[256];
				sprintf(szFile,"%s/%s",pDir,pdir->d_name);

				FILE *m_pReadFp = fopen(szFile,"r");
				if(!m_pReadFp)
				{
					printf("can not open %s\n",szFile);
					return 0;
				}

				int64_t tFirstLogTime = 0;
				int64_t tLastLogTime = 0;
				int iItems = 0;
				while(!feof(m_pReadFp))
				{
					int32_t iLogSize = 0;
					int64_t tLogTime;

					//只有read一次才知道feof...
					fread(&tLogTime,1,sizeof(u_int64_t),m_pReadFp);
					fread(&iLogSize,1,sizeof(int32_t),m_pReadFp);
					if (feof(m_pReadFp))
					{
						break;
					}
					
					int iRet = fread(pLogBuff,1,iLogSize,m_pReadFp);
					if(iRet != iLogSize)
					{
						printf("read binlog failed! ret=%d != %d,time %lld\n",iRet,iLogSize,(long long)tLogTime);
						break;
					}

					if(tLogTime < tAllFirstLogTime)
						tAllFirstLogTime = tLogTime;
					if(tLogTime > tAllLastLogTime)
						tAllLastLogTime = tLogTime;

					tLastLogTime = tLogTime;
					if(tFirstLogTime == 0)
					{
						tFirstLogTime = tLogTime;
					}	
					iItems++;
				}

				char szFirstTime[128];
				GetCurDateTimeStr(tFirstLogTime,szFirstTime);
				char szLastTime[128];
				GetCurDateTimeStr(tLastLogTime,szLastTime);				
				printf("%s  from %lld[%s] to %lld[%s], %d items\n",pdir->d_name,
										(long long)tFirstLogTime,szFirstTime,
										(long long)tLastLogTime,szLastTime,iItems);

				tAllItems += iItems;
				fclose(m_pReadFp);
			}
	    }

		char szAllFirstTime[128];
		GetCurDateTimeStr(tAllFirstLogTime,szAllFirstTime);
		char szAllLastTime[128];
		GetCurDateTimeStr(tAllLastLogTime,szAllLastTime);	
		printf("All  from %lld[%s] to %lld[%s], %lld Items\n",
										(long long)tAllFirstLogTime,szAllFirstTime,
										(long long)tAllLastLogTime,szAllLastTime,(long long)tAllItems);		
    	closedir(dp);
	}	
	else if(0 == strcmp(pAct,"cutdir"))
	{
		char *pDir = argv[2];
		int64_t iEndTime = atoll(argv[3]);

		//dir 句柄
		DIR *dp; 
		//dir 结构
		struct dirent *pdir;
		if ((dp = opendir(pDir)) == NULL)
		{
			printf("opendir %s error!\n",pDir);
			return -1;
		}

		printf("cut binlog %s\n",pDir);

		vector<string> vec_mod_file;
	    while ((pdir = readdir(dp)) != NULL)
	    {
			if(strstr(pdir->d_name,"binlog_a") || strstr(pdir->d_name,"binlog_b"))
			{
				char szFile[256];
				sprintf(szFile,"%s/%s",pDir,pdir->d_name);

				FILE *m_pReadFp = fopen(szFile,"r");
				if(!m_pReadFp)
				{
					printf("can not open %s\n",szFile);
					return 0;
				}
	
				while(!feof(m_pReadFp))
				{
					int32_t iLogSize = 0;
					int64_t tLogTime;

					//只有read一次才知道feof...
					fread(&tLogTime,1,sizeof(u_int64_t),m_pReadFp);
					fread(&iLogSize,1,sizeof(int32_t),m_pReadFp);
					if (feof(m_pReadFp))
					{
						break;
					}
					
					int iRet = fread(pLogBuff,1,iLogSize,m_pReadFp);
					if(iRet != iLogSize)
					{
						printf("read binlog failed!\n");
						break;
					}

					if(tLogTime>iEndTime)
					{
						string strFileName;
						strFileName = szFile;
						vec_mod_file.push_back(strFileName);
						break;
					}	
				}
				fclose(m_pReadFp);
			}
	    }
    	closedir(dp);
		for(int i=0; i<(int)vec_mod_file.size();i++)
		{
			string strFileName = vec_mod_file[i];
			printf("%s cut,",strFileName.c_str());
			
			char szCmd[128];
			sprintf(szCmd,"mv %s %s.bak",strFileName.c_str(),strFileName.c_str());
			system(szCmd);
			
			char szBakFile[128];
			sprintf(szBakFile,"%s.bak",strFileName.c_str());
			
			FILE *m_pReadFp = fopen(szBakFile,"r");
			if(!m_pReadFp)
			{
				printf("can not open %s\n",szBakFile);
				return 0;
			}

			FILE *m_pWriteFp = fopen(strFileName.c_str(),"w+");
			if(!m_pWriteFp)
			{
				printf("can not open %s\n",strFileName.c_str());
				return 0;
			}

			int iCnt = 0;
			while(!feof(m_pReadFp))
			{
				int32_t iLogSize = 0;
				int64_t tLogTime;

				//只有read一次才知道feof...
				fread(&tLogTime,1,sizeof(u_int64_t),m_pReadFp);
				fread(&iLogSize,1,sizeof(int32_t),m_pReadFp);
				if (feof(m_pReadFp))
				{
					break;
				}
				
				int iRet = fread(pLogBuff,1,iLogSize,m_pReadFp);
				if(iRet != iLogSize)
				{
					printf("read binlog failed!\n");
					break;
				}

				if(tLogTime<=iEndTime)
				{
					fwrite(&tLogTime,1,sizeof(u_int64_t),m_pWriteFp);
					fwrite(&iLogSize,1,sizeof(int32_t),m_pWriteFp);
					fwrite(pLogBuff,1,iLogSize,m_pWriteFp);
					iCnt++;
				}
				else
					break;
			}
			printf(" write out %d records\n",iCnt);

			fclose(m_pReadFp);
			fclose(m_pWriteFp);
		}
	}	
	return 0;
}

