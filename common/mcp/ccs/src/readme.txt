/*
������                      			 ��Ϣ                    	������

close()
                   				------ FIN ------->
FIN_WAIT1                                       						CLOSE_WAIT
                   				<----- ACK -------
FIN_WAIT2[tcp_fin_timeout=60s]
                                                       						if close()  [��Ϊ����,����һֱCLOSE_WAIT]
                   				<------ FIN ------           
                                                      						LAST_ACK
TIME_WAIT[tcp_max_tw_buckets=180000]    
                  				------ ACK ------->  

 (wait...)                                                   						CLOSED
CLOSED 

TIME_WAITʱ���Ǳ����,�����ر��ں�,���򲻿��޸�,�����޸�
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_tw_recycle = 1
ʵ�ֿ��ٻ���

�����������û�е���close,�����������Ƚ���FIN_WAIT2״̬һ��ʱ��

open files                      (-n) 1024
������ֵ��,close�ᱻ�������رղ����ܵ�����

net.ipv4.tcp_syncookies = 1  ��ֹ����DOS����
----------------------------------------------------------
������                       ��Ϣ                   						 ������

connect()
                            ------ SYN [net.ipv4.tcp_syn_retries=5(180s)]------->
SYN_SEND                                                       					SYN_RECV[tcp_max_syn_backlog=1024]
                            <----- SYN+ACK [tcp_synack_retries=5]-------
ESTABLISHED 
                            ------- ACK ------->                     
                                                                       					ESTABLISHED       


net.ipv4.tcp_max_syn_backlog = 1024
��ʾδ���Ӷ��е����������Ŀ

----------------------------------------------------------------
������                     ��Ϣ                   		 ������

SYN or FIN            ---------------->                �رյĶ˿�
				<---------RST----
----------------------------------------------------------------
/proc/sys/net/core/rmem_default	: the default size of recv socket buffer.
/proc/sys/net/core/rmem_max		: the default size of setsockopt
BDP 							: Bandwidth Delay Product 
BDP = link_bandwidth * RTT 

*/