/*
 * Copyright (c) 2009 by zhongchaoyu
 * Copyright 2009, 2009 by zhongchaoyu.  All rights reserved.
 */
 
#include <stdio.h>

#if defined(LINUX) 
#include <linux/ip.h> 
#include <linux/tcp.h> 
#else 
#include <netinet/ip.h> 
#include <netinet/tcp.h> 
#endif 
/*

public struct ip_hdr    //IP头
     {
         public byte h_lenver; //4位首部长度+4位IP版本号
         public byte tos; //8位服务类型TOS
         public ushort total_len; //16位总长度（字节）
         public ushort ident; //16位标识
         public ushort frag_and_flags; //3位标志位+13报片偏移
         public byte ttl; //8位生存时间 TTL
         public byte proto; //8位协议 (TCP, UDP 或其他)
         public ushort checksum; //16位IP首部校验和
         public uint sourceIP; //32位源IP地址
         public uint destIP; //32位目的IP地址
     }
     public struct tcp_hdr   //TCP头
     {
         public ushort th_sport; //16位源端口
         public ushort th_dport; //16位目的端口
         public uint th_seq; //32位序列号
         public uint th_ack; //32位确认号
         public byte th_lenres; //4位首部长度/6位保留字
         public byte th_flag; //6位标志位
         public ushort th_win; //16位窗口大小
         public ushort th_sum; //16位校验和
         public ushort th_urp; //16位紧急数据偏移量
     } 

struct tcphdr {
	__u16	source;
	__u16	dest;
	__u32	seq;
	__u32	ack_seq;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u16	res1:4,
		doff:4,
		fin:1,
		syn:1,
		rst:1,
		psh:1,
		ack:1,
		urg:1,
		ece:1,
		cwr:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u16	doff:4,
		res1:4,
		cwr:1,
		ece:1,
		urg:1,
		ack:1,
		psh:1,
		rst:1,
		syn:1,
		fin:1;
#else
#error	"Adjust your <asm/byteorder.h> defines"
#endif	
	__u16	window;
	__u16	check;
	__u16	urg_ptr;
};


struct udphdr {
	__u16	source;
	__u16	dest;
	__u16	len;
	__u16	check;
};

*/

//tcpdump -X -s 0 -w ./msg_file tcp  dst port 5555 and dst host 172.25.38.83 

#include "user_func.h"

CUserFunc *g_pUserFunc;

int parse_use_data(char* pData ,int iDataLen)
{
	//not push data
	if(iDataLen == 0)
		return 0;
	
	g_pUserFunc->ParseMsg(pData, iDataLen);
	return 0;
}

int decode_tcpdump_filehdr(char* pBuffer)
{
	//Tag D4 C3 B2 A1  
	return 24;
}

int decode_datalink_hdr(char* pBuffer)
{
	/*
	RFC894，包头是14字节,以太网
	RFC1042，包头是22字节,802.3
	RFC 1548，包头是5个字节,Modem拨号SLIP
	*/
	return 14;
}

int decode_ip_hdr(char* pBuffer)
{
	struct iphdr* piphdr = (struct iphdr*)pBuffer;
/*
	int ihdrlen = 20;

	//tot_len =  ip+tcp+data
	
#ifdef IP_LEN_HORDER 
	ihdrlen = piphdr->tot_len; 
#else 
	ihdrlen =ntohs( piphdr->tot_len); 
#endif 
*/	
	return piphdr->ihl*4;
}

int decode_tcp_hdr(char* pBuffer)
{
	struct tcphdr* ptcphdr = (struct tcphdr*)pBuffer;

	int ihdrlen = 20;
	
//#  if __BYTE_ORDER == __LITTLE_ENDIAN
	//ihdrlen=ptcphdr->res1*4; 
//#  elif __BYTE_ORDER == __BIG_ENDIAN
	ihdrlen=ptcphdr->doff*4;  
//#  else
//#   printf("Adjust your <bits/endian.h> defines");
//#  endif

	return ihdrlen;
}

int decode_datapkg_hdr(char* pBuffer)
{
	int *aiLen = (int*)(pBuffer+8);
	int iDataPkgLen = aiLen[0]+16;

	char *pBuff = pBuffer+16;
	pBuff += decode_datalink_hdr(pBuff);
	pBuff += decode_ip_hdr(pBuff);
	pBuff += decode_tcp_hdr(pBuff);
	
	pBuff += parse_use_data(pBuff,iDataPkgLen-(pBuff-pBuffer));
	
	return iDataPkgLen;
}

int parse_tcpdump_file(char* pBuffer,int iReadLen)
{
	char *pBuff = pBuffer;
	pBuff += decode_tcpdump_filehdr(pBuff);
	
	while(pBuff - pBuffer < iReadLen)
	{
		pBuff += decode_datapkg_hdr(pBuff);
	}
	return 0;
}

//文件头 | 数据包头 ｜ 链路层数据 ｜ 数据包头 ｜链路层数据｜ ....
int main(int argc ,char** argv)
{
	if(argc < 2)
	{
		printf("usage:%s [tcpdump file]\n",argv[0]);
		printf("tcpdump -X -s 0 -w ./msg_file tcp dst port 5555 and dst host 172.25.38.83\n");
		return 0;
	}
	char *pTcpDumpFile = argv[1];

	FILE* fp = fopen(pTcpDumpFile,"r");
	if(!fp)
	{
		printf("can not open %s\n",pTcpDumpFile);
		return -1;
	}
	
	fseek(fp,0,SEEK_END);
	int iFileLen = ftell(fp);
	char *pFileBuffer = new char[iFileLen];
	fseek(fp,0,SEEK_SET);

	int iReadLen = fread(pFileBuffer,1,iFileLen,fp);
	if(iReadLen != iFileLen)
	{
		printf("fread %s return %d\n",pTcpDumpFile,iReadLen);
		return -1;	
	}

	g_pUserFunc = new CUserFunc();
	if(parse_tcpdump_file(pFileBuffer,iReadLen))
	{
		printf("parse_tcpdump_file %s failed!\n",pTcpDumpFile);
		return -1;	
	}	

	fclose(fp);
	delete []pFileBuffer;
	return 0;
}
