#include "sockstream.hpp"

/*
pData,unDataLen:包数据

iPkgTheoryLen: 包的理论长度,在只收到部分数据的情况下就可以知道
				包长度了，0为无法判断
				
return :完整包的实际长度
*/
int my_complete_func(const void* pData, unsigned int unDataLen,int &iPkgTheoryLen)
{
	iPkgTheoryLen = 0;
	
	if (unDataLen < sizeof(int))
		return 0;
	
	iPkgTheoryLen = ntohl(*(int*)pData);
	if (iPkgTheoryLen <= (int)unDataLen)
	{
		return iPkgTheoryLen;
	}

	return 0;
}

check_complete CSockStream::net_complete_func = NULL;

CSockStream::CSockStream(int iSendBuffSize/*=5*1024*1024*/,
							int iRecvBuffSize/*=5*1024*1024*/)
{
	m_iSocketFD = -1;
	m_iStatus = STREAM_CLOSED;

	m_iReadBegin = m_iReadEnd = 0;
	m_iSendBegin = m_iSendEnd = 0;

	RECV_BUF_SIZE = iRecvBuffSize;
	m_pszRecvBuffer = new char[RECV_BUF_SIZE];

	SEND_BUF_SIZE = iSendBuffSize;
	m_pszSendBuffer = new char[SEND_BUF_SIZE];

	pthread_mutex_init(&stMutex, NULL );
}
CSockStream::~CSockStream()
{
	if( (STREAM_OPENED == m_iStatus) && m_iSocketFD > 0 )
	{
		CloseStream();
	}
	
	delete []m_pszRecvBuffer;
	delete []m_pszSendBuffer;
}

int CSockStream::SetUpStream(int iFD)
{
	if( iFD < 0 )
	{
		return -1;
	}

	if( m_iSocketFD > 0 && STREAM_OPENED == m_iStatus )
	{
		printf("Warning, setup new stream, so close the previous(%d).\n", m_iSocketFD);
		CloseStream();
	}

	int iLength = 100000;
	setsockopt(iFD, SOL_SOCKET, SO_RCVBUF, &iLength, sizeof(iLength));
	setsockopt(iFD, SOL_SOCKET, SO_SNDBUF, &iLength, sizeof(iLength));
	
	m_iSocketFD = iFD;
	m_iStatus = STREAM_OPENED;
	m_iReadBegin = m_iReadEnd = 0;
	m_iSendBegin = m_iSendEnd = 0;
	
	int iFlags;
	iFlags = fcntl(m_iSocketFD, F_GETFL, 0);
	iFlags |= O_NONBLOCK;
	iFlags |= O_NDELAY;
	fcntl(m_iSocketFD, F_SETFL, iFlags);	

	return 0;
}
int CSockStream::CloseStream()
{
	if( m_iSocketFD > 0 )
	{
		close(m_iSocketFD);
	}

	m_iSocketFD = -1;
	m_iStatus = STREAM_CLOSED;

	return 0;
}
int CSockStream::GetSocketFD()
{
	return m_iSocketFD;
}
int CSockStream::GetStatus()
{
	return m_iStatus;
}
int CSockStream::AddToCheckSet(fd_set *pCheckSet)
{
	int iTempRet = 0;

	if( !pCheckSet )
	{
		return -1;
	}

	if( m_iSocketFD > 0 && m_iStatus == STREAM_OPENED )
	{
		FD_SET( m_iSocketFD, pCheckSet );
	}
	else if( m_iSocketFD > 0 )
	{
		CloseStream();
		iTempRet = ERR_NOSOCK;
	}

	return iTempRet;
}
int CSockStream::IsFDSetted(fd_set *pCheckSet)
{
	int iTempRet = 0;

	if( !pCheckSet )
	{
		return 0;
	}

	if( m_iSocketFD > 0 && STREAM_OPENED == m_iStatus )
	{
		iTempRet = FD_ISSET( m_iSocketFD, pCheckSet );
	}
	else
	{
		iTempRet = 0;
	}

	return iTempRet;
}

int CSockStream::IsFull()
{
	if( (m_iReadEnd -m_iReadBegin) >= RECV_BUF_SIZE )
	{
		return 1;
	}
	return 0;
}

int CSockStream::GetLeftLen()
{
	return (m_iReadEnd -m_iReadBegin) ;

}

int CSockStream::RecvData()
{
	if( m_iSocketFD < 0 || STREAM_OPENED != m_iStatus )
	{
		return ERR_NOSOCK;
	}

	//调整
	if(m_iReadBegin == m_iReadEnd)
	{
		m_iReadBegin = 0;
		m_iReadEnd = 0;
	}
	else if(m_iReadEnd == RECV_BUF_SIZE)
	{
		if(m_iReadBegin > 0)
		{
			int iCodeLen = m_iReadEnd - m_iReadBegin;
			memmove(&m_pszRecvBuffer[0],&m_pszRecvBuffer[m_iReadBegin],iCodeLen);
			m_iReadBegin = 0;
			m_iReadEnd = iCodeLen;		
		}
		else
		{
			printf("The recv buffer is full now(%d, %d), stop recv data, fd = %d.\n",
							m_iReadBegin, m_iReadEnd, m_iSocketFD);
			return  ERR_AGAIN;		
		}
	}

	//循环recv的性能太低了,做一次就行了
	int iRecvedBytes = recv(m_iSocketFD, &m_pszRecvBuffer[m_iReadEnd],
		RECV_BUF_SIZE-m_iReadEnd, 0);
	if( iRecvedBytes > 0 )
	{
		m_iReadEnd += iRecvedBytes;
	}
	else if( iRecvedBytes == 0 )
	{
		printf("The peer side may closed this stream, fd = %d, errno = %d.\n", m_iSocketFD, errno);
		CloseStream();
		return ERR_SOCK_CLOSE;
	}
	else if( errno != EAGAIN )
	{
		printf("Error in read, %s, socket fd = %d.\n", strerror(errno), m_iSocketFD);
		CloseStream();
		return  ERR_FAILED;
	}

	return 0;
}

//true or false
bool CSockStream::GetOneCode(int &iCodeLength, char *pCode)
{
	if( !pCode )
		return false;

	unsigned int unDataLength = m_iReadEnd - m_iReadBegin;
	if( unDataLength <= 0 )
	{
		return false;
	}

	int iUsrCodeLen = 0;
	int iPkgTheoryLen = 0;
	if(net_complete_func)
	{
		iUsrCodeLen = net_complete_func(&m_pszRecvBuffer[m_iReadBegin],unDataLength,iPkgTheoryLen);
	}
	else
	{
		iUsrCodeLen = my_complete_func(&m_pszRecvBuffer[m_iReadBegin],unDataLength,iPkgTheoryLen);
	}
	
	if(iUsrCodeLen > 0)
	{
		iCodeLength = iUsrCodeLen;
		memcpy(pCode,&m_pszRecvBuffer[m_iReadBegin], iUsrCodeLen);
		m_iReadBegin += iUsrCodeLen;
		return true;
	}
	
	if(iPkgTheoryLen > RECV_BUF_SIZE)
	{
		printf("The pkg total len %d is too big than RECV_BUF_SIZE %d,close it!\n",iPkgTheoryLen, RECV_BUF_SIZE);
		CloseStream();
	}
	
	return false;
}

//SendOneCode(0,NULL) 是清理发送滞留数据  ,ERR_NODATA 结束
//return 0 ok
int CSockStream::SendOneCode(int iCodeLength, char *pCode)
{
	int iBytesSent = 0;
	int iTempRet = 0;
	int iLeftLen = 0;

	if( m_iSocketFD < 0 || STREAM_OPENED != m_iStatus )
	{
		return ERR_NOSOCK;
	}

	//已经没有滞留数据了
	if ((!pCode) && (m_iSendEnd == m_iSendBegin))
	{
		m_iSendBegin = m_iSendEnd = 0;
		return ERR_NODATA;
	}
	
	//首先检查是否有滞留数据
	int iBytesLeft = m_iSendEnd - m_iSendBegin;
	char *pbyTemp = &(m_pszSendBuffer[m_iSendBegin]);
	while( iBytesLeft > 0 )
	{
		iBytesSent = send(m_iSocketFD, (const char *)pbyTemp, iBytesLeft, 0);
		if( iBytesSent > 0 )
		{
			pbyTemp += iBytesSent;
			iBytesLeft -= iBytesSent;

			m_iSendBegin += iBytesSent;
		}

		if( iBytesSent < 0 && errno != EAGAIN )
		{
			//出现发送错误，关闭流
			CloseStream();
			iTempRet = ERR_FAILED;
			break;
		}
		else if( iBytesSent < 0 )
		{
			//发送缓冲已满，停止发送，剩余数据保持原位
			iTempRet = ERR_AGAIN;
			break;
		}
	}

	if( iBytesLeft == 0 )
	{
		//滞留数据发送成功，则继续发送本次提交的数据
		m_iSendBegin = m_iSendEnd = 0;
	}
	//否则，加入缓冲
	else if (pCode != NULL)
	{
		if (m_iSendBegin > 0)//SEND_BUF_SIZE/2)
		{
			memmove(m_pszSendBuffer, &m_pszSendBuffer[m_iSendBegin], m_iSendEnd-m_iSendBegin);
			m_iSendEnd = m_iSendEnd-m_iSendBegin;
			m_iSendBegin = 0;
		}

		iLeftLen = (int)(SEND_BUF_SIZE - m_iSendEnd);// - sizeof(int));
		if (iCodeLength < iLeftLen)
		{
			memcpy((void *)&(m_pszSendBuffer[m_iSendEnd]), (const void *)pCode, iCodeLength);
			m_iSendEnd += iCodeLength;
			iTempRet = 0;
		}
		else
		{
			iTempRet = ERR_PKG_TOOBIG;
		}

		return iTempRet;
	}

	if (!pCode)
	{
		return iTempRet;
	}

	//滞留数据发完, 发送本次提交的数据,此时缓冲区为空
	if (iCodeLength > (int)(SEND_BUF_SIZE/* - sizeof(int)*/))
	{
		return ERR_PKG_TOOBIG;
	}

	iBytesLeft = iCodeLength;
	pbyTemp = pCode;

	while( iBytesLeft > 0 )
	{
		iBytesSent = send(m_iSocketFD, (const char *)pbyTemp, iBytesLeft, 0);
		if( iBytesSent > 0 )
		{
			pbyTemp += iBytesSent;
			iBytesLeft -= iBytesSent;
		}
		
		if( iBytesSent < 0 && errno != EAGAIN )
		{
			//出现发送错误，关闭流
			CloseStream();
			printf("Stream send error of %d.\n", errno);
			iTempRet = ERR_FAILED;
			break;
		}
		else if( iBytesSent < 0 )
		{
			//Socket发送缓冲区满，则将剩余的数据放到缓存中
			if (iBytesLeft < SEND_BUF_SIZE)
			{
				memcpy((void *)&(m_pszSendBuffer[m_iSendEnd]), (const void *)pbyTemp, iBytesLeft);
				m_iSendEnd += iBytesLeft;
				iTempRet = 0;			
			}

			break;
		}
	}

	return iTempRet;
}
