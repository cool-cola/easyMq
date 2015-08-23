#include "Base.hpp"

char g_CharMap[][4]={
"0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16",
"17","18","19","20","21","22","23","24","25","26","27","28","29","30","31",
"32","33","34","35","36","37","38","39","40","41","42","43","44","45","46",
"47","48","49","50","51","52","53","54","55","56","57","58","59","60","61",
"62","63","64","65","66","67","68","69","70","71","72","73","74","75","76",
"77","78","79","80","81","82","83","84","85","86","87","88","89","90","91",
"92","93","94","95","96","97","98","99","100","101","102","103","104","105"
,"106","107","108","109","110","111","112","113","114","115","116","117","118","119",
"120","121","122","123","124","125","126","127","128","129","130","131","132","133",
"134","135","136","137","138","139","140","141","142","143","144","145","146","147",
"148","149","150","151","152","153","154","155","156","157","158","159","160","161",
"162","163","164","165","166","167","168","169","170","171","172","173","174","175",
"176","177","178","179","180","181","182","183","184","185","186","187","188","189",
"190","191","192","193","194","195","196","197","198","199","200","201","202","203",
"204","205","206","207","208","209","210","211","212","213","214","215","216","217",
"218","219","220","221","222","223","224","225","226","227","228","229","230","231",
"232","233","234","235","236","237","238","239","240","241","242","243","244","245",
"246","247","248","249","250","251","252","253","254","255"
};

void INET_NTOA(unsigned int unIP,char *pIP) 
{ 
	strcpy((char*)(pIP),g_CharMap[*(unsigned char*)&(unIP)]);\
	strcat((char*)(pIP),".");\
	strcat((char*)(pIP),g_CharMap[*((unsigned char*)&(unIP)+1)]);\
	strcat((char*)(pIP),".");\
	strcat((char*)(pIP),g_CharMap[*((unsigned char*)&(unIP)+2)]);\
	strcat((char*)(pIP),".");	\
	strcat((char*)(pIP),g_CharMap[*((unsigned char*)&(unIP)+3)]);\
}

void HtoP (unsigned int unIP,char *pIP)
{
	INET_NTOA(htonl(unIP),pIP);
}

//like sprintf
char* uint2str(uint32_t n,char *p) 
{
	int c;
	if(n<10) { c = 1; goto len1; }
	if(n<100) { c = 2; goto len2; }
	if(n<1000) { c = 3; goto len3; }
	if(n<10000) { c = 4; goto len4; }
	if(n<100000) { c = 5; goto len5; }
	if(n<1000000) { c = 6; goto len6; }
	if(n<10000000) { c = 7; goto len7; }
	if(n<100000000) { c = 8; goto len8; }
	if(n<1000000000) { c = 9; goto len9; }
	c = 10;
	p[9] = 0x30 | (n%10); n/=10;
len9:	p[8] = 0x30 | (n%10); n/=10;
len8:	p[7] = 0x30 | (n%10); n/=10;
len7:	p[6] = 0x30 | (n%10); n/=10;
len6:	p[5] = 0x30 | (n%10); n/=10;
len5:	p[4] = 0x30 | (n%10); n/=10;
len4:	p[3] = 0x30 | (n%10); n/=10;
len3:	p[2] = 0x30 | (n%10); n/=10;
len2:	p[1] = 0x30 | (n%10); n/=10;
len1:	p[0] = 0x30 | n;
	p[c] = 0;
	return p+c;
}

//like sprintf
char* int2str(int32_t n,char *p) 
{
	int c;
	if(n<0) { *p++ = '-'; n=-n; }
	if(n<10) { c = 1; goto len1; }
	if(n<100) { c = 2; goto len2; }
	if(n<1000) { c = 3; goto len3; }
	if(n<10000) { c = 4; goto len4; }
	if(n<100000) { c = 5; goto len5; }
	if(n<1000000) { c = 6; goto len6; }
	if(n<10000000) { c = 7; goto len7; }
	if(n<100000000) { c = 8; goto len8; }
	if(n<1000000000) { c = 9; goto len9; }
	c = 10;
	p[9] = 0x30 | (n%10); n/=10;
len9:	p[8] = 0x30 | (n%10); n/=10;
len8:	p[7] = 0x30 | (n%10); n/=10;
len7:	p[6] = 0x30 | (n%10); n/=10;
len6:	p[5] = 0x30 | (n%10); n/=10;
len5:	p[4] = 0x30 | (n%10); n/=10;
len4:	p[3] = 0x30 | (n%10); n/=10;
len3:	p[2] = 0x30 | (n%10); n/=10;
len2:	p[1] = 0x30 | (n%10); n/=10;
len1:	p[0] = 0x30 | n;
	p[c] = 0;
	return p+c;
}

bool IsChar(char c) 
{ 
	if((c >= 32) &&  (c <= 126))
		return true;
	
	return false;
} 

/*
Big-Endian and Little-Endian is depend on your CPU 
Intel X86 is little endian
#include <endian.h>	__BIG_ENDIAN __LITTLE_ENDIAN __BYTE_ORDER
*/
bool IsCPULittleEndian() 
{ 
	const int n = 1; 
	return *((char*)&n) ? true : false;
} 

int ex(int i)
{
    char *a1 = (char*)&i;
    char *a2 = (char*)&i+1;
    char *a3 = (char*)&i+2;
    char *a4 = (char*)&i+3;

    char t = *a4;
    *a4 = *a1;
    *a1 = t;

    t = *a2;
    *a2=*a3;
    *a3=t;
    return i;
}

u_int64_t NTOHL64(u_int64_t N)
{
	const int n = 1;
	bool bLittleEndian = *((char*)&n) ? true : false;
	if(!bLittleEndian)
		return N;
	
	u_int64_t	H = 0;
	u_int32_t iTmp = 0;
	
	iTmp = ntohl((u_int32_t)(N>>32));
	memcpy(&H,&iTmp,sizeof(u_int32_t));
	
	iTmp = ntohl((u_int32_t)(N));
	memcpy(((char*)&H)+sizeof(u_int32_t),&iTmp,sizeof(u_int32_t));
	
	return H;
}

u_int64_t HTONL64(u_int64_t H)
{
	const int n = 1;
	bool bLittleEndian = *((char*)&n) ? true : false;
	if(!bLittleEndian)
		return H;
	
	u_int64_t	N = 0;
	u_int32_t iTmp = 0;
	
	iTmp = htonl((u_int32_t)(H>>32));
	memcpy(&N,&iTmp,sizeof(u_int32_t));
	
	iTmp = htonl((u_int32_t)(H));
	memcpy(((char*)&N)+sizeof(u_int32_t),&iTmp,sizeof(u_int32_t));
	
	return N;
}

void PrintBin2Buff(string &strBuff,char *pBuffer, int iLength)
{
    if( iLength <= 0 || pBuffer == NULL )
        return;

	if(iLength > 2048)
	{
		iLength = 512;
	}

	char szTmp[10240]={0};
    for(int i = 0; i < iLength; i++ )
    {
        if( !(i%16) )
        {
			sprintf(szTmp+strlen(szTmp),"\n%04d>    ", i/16+1);
        }
		sprintf(szTmp+strlen(szTmp),"%02x ", (unsigned char)pBuffer[i]);
    }
    sprintf(szTmp+strlen(szTmp),"\n");

	strBuff.assign(szTmp,strlen(szTmp));
}

void PrintBin(FILE* pLogfp,char *pBuffer, int iLength)
{
    if( iLength <= 0 || pBuffer == NULL )
        return;

    for(int i = 0; i < iLength; i++ )
    {
        if( !(i%16) )
        {
		fprintf(pLogfp,"\n%04d>    ", i/16+1);
        }
	fprintf(pLogfp,"%02x ", (unsigned char)pBuffer[i]);
    }
    fprintf(pLogfp,"\n");
}

void PrintHexStr(FILE* pLogfp,char *pBuffer, int iLength)
{
    if( iLength <= 0 || pBuffer == NULL )
        return;

    for(int i = 0; i < iLength; i++ )
    {
	fprintf(pLogfp,"%02x", (unsigned char)pBuffer[i]);
    }
}

void GetDateTimeStr(time_t tNow,char* szTimeStr)
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

//从 低地址到高地址对应
void HexStr2Bin(char* pStrHex,string& strBin)
{
	char szHexItem[3];
	memset(szHexItem,0,sizeof(szHexItem));

	int iVal;
	int iHexLen = strlen(pStrHex);
	if(iHexLen % 2)
	{
		memcpy(szHexItem,pStrHex,1);
		sscanf(szHexItem, "%x", &iVal);
		char cVal = iVal;
		strBin.append((char*)&cVal, sizeof(char));
		pStrHex++;
	}
	
	iHexLen = strlen(pStrHex);
	char* pPos = pStrHex;
	while((pPos - pStrHex) < iHexLen)
	{
		memcpy(szHexItem,pPos,2);

		int iVal;
		sscanf(szHexItem, "%x", &iVal);
		char cVal = iVal;

		strBin.append((char*)&cVal, sizeof(char));
		pPos += 2;
	}
}
void GetNameFromPath(char *szPath,char* szName)
{
	int iLen = strlen(szPath);
	if (iLen<=0)
	{
		szName[0] = 0;
		return;
	}
	
	char *p = szPath+iLen-1;
	while((*p != '/')&&(p!=szPath))
	{
		p--;
	}
	strcpy(szName,p+1);
	return;
};

char* CreateShm(key_t iShmKey,size_t iShmSize,int &iErrNo,bool &bNewCreate,int iForceAttach/*=0*/)
{
	int iShmID = shmget( iShmKey,iShmSize,IPC_CREAT|IPC_EXCL|0666);
	if( iShmID < 0)
	{
		bNewCreate = false;
		if(errno != EEXIST)
		{
			iErrNo = errno;
			return NULL;
		}

		iShmID = shmget( iShmKey,iShmSize,0666);
		if( iShmID < 0)
		{
			if (!iForceAttach)
			{
				iErrNo = errno;
				return NULL;
			}
			
			iShmID = shmget( iShmKey,0,0666);
			if( iShmID < 0)
			{
				iErrNo = errno;
				return NULL;
			}
			else
			{
				if( shmctl( iShmID,IPC_RMID,NULL))
				{
					iErrNo = errno;
					return NULL;
				}
				iShmID = shmget( iShmKey,iShmSize,IPC_CREAT|IPC_EXCL|0666);
				if( iShmID < 0)
				{
					iErrNo = errno;
					return NULL;
				}
				bNewCreate = true;
			}
		}
	}
	else
	{
		bNewCreate = true;
	}

	//if shall not be attached, shmat() shall return -1	
	char *pShmPtr = (char *)shmat( iShmID,NULL,0);
	if(pShmPtr == (char*)-1)
	{
		iErrNo = errno;
		return NULL;
	}

	iErrNo = 0;
	return pShmPtr;
}

/*
/proc/sys/net/core/rmem_default	: the default size of recv socket buffer.
/proc/sys/net/core/rmem_max		: the default size of setsockopt
BDP 							: Bandwidth Delay Product 
BDP = link_bandwidth * RTT 
Protocol: 0 tcp,1 udp
*/
int CreateListenSocket(u_int32_t unListenIP,uint16_t usListenPort,
					int RCVBUF/*=-1*/,int SNDBUF/*=-1*/,
					int iProtocol/*=0*/,int iListenBackLog/*=1024*/)
{
	int iSocket = -1;
	if(iProtocol == 0)
		iSocket = socket(AF_INET,SOCK_STREAM,0);
	else
		iSocket = socket(AF_INET,SOCK_DGRAM,0);
	
	if (iSocket < 0)
	{
		printf("[%s] socket create error, errno%d!",__FUNCTION__, errno);
		return -1;
	}

	int iRet = fcntl(iSocket,F_GETFL);
	if (iRet < 0)
	{
		close(iSocket);
		printf("ERR:fcntl() failed, errno %d\n", errno);
		return -2;
	}

/*
#define O_NONBLOCK	  04000
#define O_NDELAY		O_NONBLOCK
*/
	fcntl(iSocket,F_SETFL,iRet | O_NONBLOCK | O_NDELAY);

	/*
	SO_KEEPALIVE	解决死链接问题
	SO_LINGER		(关闭)选项可以作用在套接口上，来使得程序阻塞
					close函数调用，直到所有最后的数据传送到远程端。
	TCP_NODELAY 	禁用Nagle 算法,这个必须禁用!!
	*/
	int flags = 1;
	struct linger ling = {0,0};
	setsockopt(iSocket,SOL_SOCKET,SO_REUSEADDR,&flags,sizeof(flags));
	setsockopt(iSocket,SOL_SOCKET,SO_KEEPALIVE,&flags,sizeof(flags));
	setsockopt(iSocket,SOL_SOCKET,SO_LINGER,&ling,sizeof(ling));
	setsockopt(iSocket,IPPROTO_TCP,TCP_NODELAY,(char *)&flags,sizeof(flags));

	 struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons((int16_t)usListenPort);
	addr.sin_addr.s_addr = (u_int32_t)unListenIP;
	if (bind(iSocket,(struct sockaddr *) &addr,sizeof(addr)) == -1)
	{
		close(iSocket);
		printf("ERR:bind() failed!may be port has been bind by others, errno %d\n", errno);
		return -3;
	}

	/*
	the value is limited by OS,no matter what user set.
	
	rdefault 	87380 , 			wdefault 	16384 ,
	rmin 	256 , 			wmin 	2048 ,
	rmax,	262142			wmax 	262142
	
	TCP window size is swaped to each other by SYN when connecting.
	so SO_RCVBUF must be set before connect() or listen();	
	*/
	int iOptLen = sizeof(int);
	if(SNDBUF != -1)
		setsockopt(iSocket,SOL_SOCKET,SO_SNDBUF,(const void *)&SNDBUF,(socklen_t)iOptLen);

	if(RCVBUF != -1)
		setsockopt(iSocket,SOL_SOCKET,SO_RCVBUF,(const void *)&RCVBUF,(socklen_t)iOptLen);	

	//listen
	if((iProtocol==0) && (-1 == listen(iSocket,iListenBackLog)))
	{
		close(iSocket);
		printf("ERR:listen error, errno %d!\n", errno);
		return -4;
	}
	return iSocket;
}

int CreateConnectSocket(u_int32_t unDestIP,uint16_t usDestPort,
								int RCVBUF/*=-1*/,int SNDBUF/*=-1*/,
								int iProtocol/*=0*/)
{
	int iSocket = -1;
	if(iProtocol == 0)
		iSocket = socket(AF_INET,SOCK_STREAM,0);
	else
		iSocket = socket(AF_INET,SOCK_DGRAM,0);
	
	if (iSocket < 0)
	{
		printf("[%s] socket create error, errno%d!",__FUNCTION__, errno);
		return -1;
	}

	int iRet = fcntl(iSocket,F_GETFL);
	if (iRet < 0)
	{
		close(iSocket);
		return -2;
	}
	fcntl(iSocket,F_SETFL,iRet | O_NONBLOCK | O_NDELAY);
	
	struct linger ling = {0,0};
	setsockopt(iSocket,SOL_SOCKET,SO_LINGER,&ling,sizeof(ling));
	int flags = 1;
	setsockopt(iSocket,IPPROTO_TCP,TCP_NODELAY,(char *)&flags,sizeof(flags));

	/*
	the value is limited by OS,no matter what user set.
	
	rdefault 	87380 , 			wdefault 	16384 ,
	rmin 	256 , 			wmin 	2048 ,
	rmax,	262142			wmax 	262142

	TCP window size is swaped to each other by SYN when connecting.
	so SO_RCVBUF must be set before connect() or listen();
	*/
	int iOptLen = sizeof(int);
	if(SNDBUF != -1)
		setsockopt(iSocket,SOL_SOCKET,SO_SNDBUF,(const void *)&SNDBUF,(socklen_t)iOptLen);

	if(RCVBUF != -1)
		setsockopt(iSocket,SOL_SOCKET,SO_RCVBUF,(const void *)&RCVBUF,(socklen_t)iOptLen);	

	if(usDestPort == 0)
		return iSocket;
	
	struct sockaddr_in svraddr;
	memset(&svraddr,0,sizeof(svraddr));
	svraddr.sin_family = AF_INET;
	svraddr.sin_port = htons(usDestPort);
	svraddr.sin_addr.s_addr = unDestIP;
	int ret = connect(iSocket, (struct sockaddr *)&svraddr,sizeof(svraddr));
	if ((ret != 0)&& (errno != EWOULDBLOCK) && (errno != EINPROGRESS))
	{
		close(iSocket);
		return -1;
	}
	return iSocket;
}

bool IsIpAddr(char* pHostStr)
{
	//判断是否是ip地址
	bool isIPAddr = true;
	int tmp1,tmp2,tmp3,tmp4,i;
	while( 1)
	{
		i = sscanf(pHostStr,"%d.%d.%d.%d",&tmp1,&tmp2,&tmp3,&tmp4);

		if( i != 4)
		{
			isIPAddr = false;
			break;
		}

		if( (tmp1 > 255) || (tmp2 > 255) || (tmp3 > 255) || (tmp4 > 255))
		{
			isIPAddr = false;
			break;
		}

		for( const char *pChar = pHostStr; *pChar != '\0'; pChar++)
		{
			if( (*pChar != '.') && ((*pChar < '0') || (*pChar > '9')))
			{
				isIPAddr = false;
				break;
			}
		}
		break;
	}

	return isIPAddr;
}
bool isSocketAlive(int iSocket)
{
    int  iLen;
    char acBuff[10];
    
    if (iSocket <= 0)
        return false;

    /* save the flags */
    int flags;
    flags =  fcntl(iSocket, F_GETFL, 0);

    /*set the socket to NONBLOCK*/
    if (fcntl(iSocket, F_SETFL, flags | O_NONBLOCK) < 0)
    {       
       close(iSocket);
       return false;    
    }

    /*check if iSocket has been closed? */
    iLen = recv(iSocket, acBuff, 1, MSG_PEEK); 
    if (iLen == 0)
    {
        close(iSocket);
        return false;
    }
    else if (iLen > 0)
    {
        /* restore the flags of socket*/
        if (fcntl(iSocket, F_SETFL, flags) < 0)
        {
            close(iSocket);
            return false;
        }
        
        return true;    
    }
    else  //iLen < 0
    {
        if ((iLen == -1) && 
            ((errno == EAGAIN) || (errno == EINTR) || (errno == EWOULDBLOCK)))
        {
            /* restore the flags of  socket*/
            if (fcntl(iSocket, F_SETFL, flags) < 0)
            {
                close(iSocket);
                return false;
            }
            
            return true;
        }
        else
        {
            close(iSocket);
            return false;
        }
    }
}

//--------------------------------------------------------------------------

int LoadConf(char* pConfFile,std::vector<std::string> &vecConf)
{
	FILE *pFileFp = fopen(pConfFile,"r");
	if(!pFileFp)
	{
		printf("Can not open file %s\n",pConfFile);
		return -1;
	}
	vecConf.clear();

	std::string strLine;
	char szLine[1024];
	while(NULL != fgets(szLine,sizeof(szLine)-1,pFileFp))	
	{
		int i=0;
		while(szLine[i] == ' ' || szLine[i] == '\t') 	i++;

		if(szLine[i] == '#' || szLine[i] == '\0' || szLine[i] == '\r' ||szLine[i] == '\n')
		{
			continue;
		}
		
		strLine = (char*)&szLine[i];
		string strCfgLine = trim_right(trim_left(strLine," \t")," \t\r\n");
		
		if(strCfgLine.length())
			vecConf.push_back(strCfgLine);
	}

	fclose(pFileFp);
	return 0;
}

//split  pContentLine by pSplitTag, output to vecContent
int SplitTag(char* pContentLine,char* pSplitTag,vector<string> &vecContent)
{
	char *pStartPtr = pContentLine;
	int iTagLen = strlen(pSplitTag);

	while(1)
	{
		char* pTagPtr = strstr(pStartPtr,pSplitTag);
		if(!pTagPtr)
		{
			string strTag = pStartPtr;
			vecContent.push_back(trim_right(trim_left(strTag," \t")," \t\r\n"));
			break;
		}

		string strTag(pStartPtr,pTagPtr-pStartPtr);

		vecContent.push_back(trim_right(trim_left(strTag," \t")," \t\r\n"));
		pStartPtr = pTagPtr + iTagLen;	

		if(*pStartPtr == 0)
			break;
	}
	return 0;
}

/*
	std::vector<std::vector<std::string> >vecCfgItem;
	LoadGridCfgFile("aaa.conf",vecCfgItem);

	for(int i=0; i<vecCfgItem.size();i++)
	{
		std::vector<std::string> &vecline = vecCfgItem[i];
		for (int j=0; j<vecline.size();j++)
		{
			printf("%s,",vecline[j].c_str());
		}
		printf("\n");
	}
*/
int LoadGridCfgFile(char* pConfFile,std::vector<std::vector<std::string> >&vecCfgItem)
{
	std::vector<std::string> vecConfLine;
	if(LoadConf(pConfFile,vecConfLine))
		return -1;

	char szLine[2048];
	for (int i=0; i<(int)vecConfLine.size(); i++)
	{
		strcpy(szLine,(char*)vecConfLine[i].c_str());
		int iLineLen = strlen(szLine);
		
		// tab -> blank
		for(int j=0; j<iLineLen; j++)
		{
			if(szLine[j] == '\t')
				szLine[j] = ' ';
		}

		//muti blank -> blank
		for(int j=0; j<iLineLen-1; j++)
		{
			if((szLine[j] == ' ') && (szLine[j+1] == ' '))
			{
				memmove(&szLine[j],&szLine[j+1],iLineLen-j-1);
				szLine[iLineLen-1] = 0;
				iLineLen--;
				j--;
			}
		}
		
		vector<string> vecTags;
		SplitTag(szLine," ",vecTags);
		
		vecCfgItem.push_back(vecTags);
	}
	return 0;

}
int DumpGridCfgFile(std::vector<std::vector<std::string> >&vecCfgItem,char* pConfFile)
{
	FILE *pFileFp = fopen(pConfFile,"w+");
	if(!pFileFp)
	{
		printf("Can not open file %s\n",pConfFile);
		return -1;
	}
	
	char szLine[2048];
	for (int i=0; i<(int)vecCfgItem.size(); i++)
	{
		szLine[0] = 0;
		std::vector<std::string> &vecConfLine = vecCfgItem[i];
		for(int j=0; j<(int)vecConfLine.size();j++)
		{
			std::string &strItem = vecConfLine[j];
			strcat(szLine,strItem.c_str());
			if(j != (int)(vecConfLine.size()-1))
			{
				strcat(szLine,"\t");
			}
		}
		fprintf(pFileFp,"%s\n",szLine);
	}
	fclose(pFileFp);
	return 0;
}

void TrimHeadTail(char* pContentLine)
{
	int iLen = strlen(pContentLine);
	//tail
	for (int i=iLen-1; i>=0; i--)
	{
		if((pContentLine[i] == '\r') ||(pContentLine[i] == '\n') ||(pContentLine[i] == '\t') ||
				(pContentLine[i] == ' '))
		{
			pContentLine[i] = 0;
		}
		else
			break;
	}

	//head
	iLen = strlen(pContentLine);
	char *pPos=pContentLine;
	while(1)
	{
		if(*pPos == 0)
			break;
		
		if((*pPos == '\r') ||(*pPos== '\n') ||(*pPos == '\t') ||(*pPos == ' '))
		{
			pPos++;
		}
		else
			break;
	}
	if(pPos != pContentLine)
	{
		memmove(pContentLine,pPos,iLen-(pPos-pContentLine)+1);
	}
}
long long TimeDiff(struct timeval t1, struct timeval t2)
{
	long long t = 0;
	
	if (t1.tv_sec == t2.tv_sec)
		t = t1.tv_usec - t2.tv_usec;
	else
		t = (long long)(t1.tv_sec - t2.tv_sec) * 1000000 + (t1.tv_usec - t2.tv_usec);

	return t;
}


//--------------------------------------------------------------------------
/*
总时间轴长度iTimeAllSpanMs毫秒
检测粒度iEachGridSpanMs 毫秒
总请求容量iMaxNumInAllSpan个
*/
CLoadGrid::CLoadGrid(int iTimeAllSpanMs,int iEachGridSpanMs,int iMaxNumInAllSpan/*=0*/)
{
	m_iTimeAllSpanMs = iTimeAllSpanMs;
	m_iEachGridSpanMs = iEachGridSpanMs;
	m_iMaxNumInAllSpan = iMaxNumInAllSpan;
	
	m_iNumInAllSpan = 0;
	m_iCurrGrid = 0;

	m_iAllGridCount = (int)(m_iTimeAllSpanMs/(float)m_iEachGridSpanMs + 0.9);
	m_iGridArray = new int[m_iAllGridCount];
	memset(m_iGridArray,0,m_iAllGridCount*sizeof(int));

	gettimeofday(&m_tLastGridTime,NULL);

	m_bLock = false;
	spin_lock_init(&m_spinlock_t);
}

CLoadGrid::~CLoadGrid()
{ 
	delete []m_iGridArray;
}

/*
根据当前时间更新内部时间轴
*/
void CLoadGrid::UpdateLoad(timeval *ptCurrTime)
{
	if(m_iTimeAllSpanMs <= 0 || m_iEachGridSpanMs <= 0)
		return;

	timeval tCurrTime;
	if(ptCurrTime)
		memcpy(&tCurrTime,ptCurrTime,sizeof(timeval));
	else
		gettimeofday(&tCurrTime,NULL);

	//流逝的时间us
	int iTimeGoUs = (tCurrTime.tv_sec -m_tLastGridTime.tv_sec)*1000000
						+ tCurrTime.tv_usec -m_tLastGridTime.tv_usec;

	//时间异常或流逝的时间超过总时间轴而过期
	if((iTimeGoUs < 0) || (iTimeGoUs >= m_iTimeAllSpanMs*1000))
	{
		//全部清理
		memset(m_iGridArray,0,m_iAllGridCount*sizeof(int));
		m_iNumInAllSpan = 0;
		m_iCurrGrid = 0;	
		gettimeofday(&m_tLastGridTime,NULL);
		return;
	}
	
	//超过1个时间片
	int iEachGridSpanUs = m_iEachGridSpanMs*1000;
	if (iTimeGoUs > iEachGridSpanUs)
	{
		//流逝掉iGridGoNum 个时间片段,首尾循环
		int iGridGoNum = iTimeGoUs/iEachGridSpanUs;
		for (int i=0; i<iGridGoNum; i++)
		{
			m_iCurrGrid++;
			if (m_iCurrGrid == m_iAllGridCount)
			{
				m_iCurrGrid = 0;
			}	
			m_iNumInAllSpan -= m_iGridArray[m_iCurrGrid];
			m_iGridArray[m_iCurrGrid] = 0;	
		}

		//更新最后记录的时间
		int iTimeGoGrid = iGridGoNum*iEachGridSpanUs;
		int iSec = (iTimeGoGrid/1000000);
		int iuSec = (iTimeGoGrid%1000000);

		m_tLastGridTime.tv_sec += iSec;
		m_tLastGridTime.tv_usec += iuSec;
		if (m_tLastGridTime.tv_usec > 1000000)
		{
			m_tLastGridTime.tv_usec -= 1000000;
			m_tLastGridTime.tv_sec++;
		}		
	}
	//未超过一个时间片，不需要更新
	else
	{
		;
	}
}

int CLoadGrid::CheckLoad(timeval tCurrTime)
{
	int iRet;
	
	if(m_bLock)	spin_lock(&m_spinlock_t);
	
	do
	{
		if(m_iTimeAllSpanMs <= 0 || m_iEachGridSpanMs <= 0)
		{
			iRet = LR_NORMAL;
			break;
		}

		//根据当前时间更新时间轴
		UpdateLoad(&tCurrTime);

		//累加本次
		m_iNumInAllSpan++;
		m_iGridArray[m_iCurrGrid]++;
	
		//到达临界
		if (m_iMaxNumInAllSpan && (m_iNumInAllSpan > m_iMaxNumInAllSpan))
			iRet =  LR_FULL;
		else
			iRet =  LR_NORMAL;
	}while(0);

	if(m_bLock)	spin_unlock(&m_spinlock_t);
	
	return iRet;
}

/*
获取当前时间轴数据，获取之前要根据当前时间更新，否则
得到的是最后时刻的情况
*/
void CLoadGrid::FetchLoad(int& iTimeAllSpanMs,int& iReqNum)
{
	if(m_bLock)	spin_lock(&m_spinlock_t);
	
	UpdateLoad();
	iTimeAllSpanMs = m_iTimeAllSpanMs;
	iReqNum = m_iNumInAllSpan;	

	if(m_bLock)	spin_unlock(&m_spinlock_t);
}
//---------------------------------------------------------------


