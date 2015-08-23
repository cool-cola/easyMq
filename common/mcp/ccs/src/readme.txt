/*
主动方                      			 消息                    	被动方

close()
                   				------ FIN ------->
FIN_WAIT1                                       						CLOSE_WAIT
                   				<----- ACK -------
FIN_WAIT2[tcp_fin_timeout=60s]
                                                       						if close()  [人为驱动,否则一直CLOSE_WAIT]
                   				<------ FIN ------           
                                                      						LAST_ACK
TIME_WAIT[tcp_max_tw_buckets=180000]    
                  				------ ACK ------->  

 (wait...)                                                   						CLOSED
CLOSED 

TIME_WAIT时间是编译宏,除非重编内核,否则不可修改,但可修改
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_tw_recycle = 1
实现快速回收

如果被动进程没有调用close,则主动方被迫进入FIN_WAIT2状态一段时间

open files                      (-n) 1024
超过此值后,close会被调用来关闭不接受的连接

net.ipv4.tcp_syncookies = 1  防止部分DOS攻击
----------------------------------------------------------
主动方                       消息                   						 被动方

connect()
                            ------ SYN [net.ipv4.tcp_syn_retries=5(180s)]------->
SYN_SEND                                                       					SYN_RECV[tcp_max_syn_backlog=1024]
                            <----- SYN+ACK [tcp_synack_retries=5]-------
ESTABLISHED 
                            ------- ACK ------->                     
                                                                       					ESTABLISHED       


net.ipv4.tcp_max_syn_backlog = 1024
表示未连接队列的最大容纳数目

----------------------------------------------------------------
主动方                     消息                   		 被动方

SYN or FIN            ---------------->                关闭的端口
				<---------RST----
----------------------------------------------------------------
/proc/sys/net/core/rmem_default	: the default size of recv socket buffer.
/proc/sys/net/core/rmem_max		: the default size of setsockopt
BDP 							: Bandwidth Delay Product 
BDP = link_bandwidth * RTT 

*/