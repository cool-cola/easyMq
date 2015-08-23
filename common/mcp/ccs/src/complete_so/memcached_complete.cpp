#include "protocol_binary.h"
#include <string.h>
#include <stdlib.h>
#ifdef MEMCACHED_DEBUG_ENABLE
	#define MEMCACHED_DEBUG(x...) printf(x)
#else
	#define MEMCACHED_DEBUG(x...)
#endif
#define MAX_TOKENS 8
typedef struct token_s 
{
		char *value;
			size_t length;
} token_t;
int safe_strtol(token_t* token, int32_t *out) 
{
	char *endptr;
	char str[12];
	long l = 0;
	
	if(!token || !token->length || token->length > 11)
	{
		return 1;
	}
	*out = 0;
	memcpy(str, token->value, token->length);
	str[token->length] = 0;
	l = strtol(str, &endptr, 10);

	if ((*endptr==' ') || (*endptr == '\0' && endptr != str)) {
		*out = l;
		return 1;
	}
	
	return 0;
}

int tokenize_command(char *command, int cmd_len, token_t *tokens, int max_tokens, int *first_n_tokens) 
{
	char *s, *e;
	int ntokens = 0, first_n = 0;
	int i = 0;
	
	*first_n_tokens = 0;
	s = e = command;
	i = 0;
	while(*s == ' ')
	{
		++s;
		++i;
	}
	e = s;
	for (; (ntokens < max_tokens) && (i < cmd_len); ++e, ++i) 
	{
		if (*e == ' ' || *e == '\n' ||*e == '\r') 
		{
			if (s != e) 
			{
				tokens[ntokens].value = s;
				tokens[ntokens].length = e - s;
				++ntokens;
				++first_n;						
			}
			s = e + 1;
			if(*e == '\n')
			{
				if(i < cmd_len - 1)
				{
					tokens[ntokens].value = s;
					tokens[ntokens].length = cmd_len - 1 - i;
					++ntokens;
				}
				break;
			}
		}
	}
	*first_n_tokens = first_n;
	return ntokens;
}

int memcached_data_complete(const void* data, unsigned int data_len, int *i_pkg_theory_len)
{
	char *ptr = (char*)data;
	int ntokens = 0, first_ntokens = 0, vlen = 0;
	token_t tokens[MAX_TOKENS];
//	char buf[128];
	
	if(ptr[1] == 't')//stats
	{
		MEMCACHED_DEBUG("command stats\n");
		goto _send_up;
	}
	
	if(memcmp(ptr, "add ", sizeof("add ") - 1) &&
		memcmp(ptr, "append ", sizeof("append ") - 1) &&
		memcmp(ptr, "prepend ", sizeof("prepend ") - 1) &&
		memcmp(ptr, "replace ", sizeof("replace ") - 1) &&
		memcmp(ptr, "set ", sizeof("set ") - 1) &&
		memcmp(ptr, "cas ", sizeof("cas ") - 1))
	{
		MEMCACHED_DEBUG("unkown command\n");
		goto _send_up;
	}

	ntokens = tokenize_command(ptr, data_len, tokens, MAX_TOKENS, &first_ntokens);
	MEMCACHED_DEBUG("tokens %d first ntokens %d\n", ntokens, first_ntokens);
	if(first_ntokens < 4)
	{
		MEMCACHED_DEBUG("error command num first %d total %d\n", first_ntokens, ntokens);
		goto _send_up;
	}
	
	if(ntokens != (first_ntokens + 1))
	{
		return 0;
	}
	
	if(!safe_strtol(&tokens[4], (int32_t *)&vlen))
	{
		MEMCACHED_DEBUG("error data length\n");
		goto _send_up;
	}
	
	if(ptr[0] != 'c' )
	{
		if(first_ntokens != 4 && first_ntokens != 5)//not cas
		{	
			MEMCACHED_DEBUG("first ntokens %d != 4 && != 5\n", first_ntokens);
			goto _send_up;
		}

	}
	else
	{
		if(first_ntokens != 5 && first_ntokens != 6)//cas
		{
			MEMCACHED_DEBUG("cas ntokens %d != 5 && != 6 \n", first_ntokens);
			goto _send_up;
		}
	}
	if((int)tokens[first_ntokens].length >= (int)(vlen + 2))
	{
		MEMCACHED_DEBUG("data len %d, need len %d\n", tokens[first_ntokens].length, vlen);
		goto _send_up;
	}

	return 0;
	
_send_up:
	#ifdef MEMCACHED_DEBUG_ENABLE
//	memcpy(buf, (char*)data, sizeof(buf) - 1);
//	buf[sizeof(buf) - 1] = 0;	
//	MEMCACHED_DEBUG ("send [%d] bytes %s to app", data_len, buf);
	#endif
	return data_len;	
}

#include <arpa/inet.h>
#include <string.h>

extern "C"
{
int net_complete_func(const void* data, unsigned int data_len, int &i_pkg_theory_len)
{
	
	protocol_binary_request_no_extras *req;

	//char buf[128];
	int i;
	
	i_pkg_theory_len = 0;
	MEMCACHED_DEBUG("enter memcached_complete_func %p, %d\n", data, data_len);
	if(data_len < 1)
	{
		return 0;
	}
	
	if(((unsigned char*)data)[0] == PROTOCOL_BINARY_REQ)
	{
		/*binary*/	
		if(data_len < (unsigned int)sizeof(protocol_binary_request_no_extras))
		{
			return 0;
		}
		req = (protocol_binary_request_no_extras*)data;
		i_pkg_theory_len = ntohl(req->message.header.request.bodylen) + (int)sizeof(protocol_binary_request_no_extras);
		MEMCACHED_DEBUG("theory len %d\n", i_pkg_theory_len);
		if(i_pkg_theory_len <= (int)data_len)
		{	
			MEMCACHED_DEBUG("send %d to app\n", data_len);
			return i_pkg_theory_len;
		}
		
	}
	else
	{
		/*ascii text*/
		char *ptr = (char*)data;
		if(ptr[data_len - 1] != '\n')
		{
			return 0;
		}
		else
		{
			i = 0;
			while(i < (int)data_len && *ptr == ' ')
			{
				ptr++;
				i++;
				if(i > 20)
				{
					MEMCACHED_DEBUG("head spaces %d > 20\n", i);
					goto _send_up;
				}
			}
			
			if(data_len < 3)
			{
				MEMCACHED_DEBUG("data_len %d < 3\n", data_len);
				return data_len;
			}
			switch(ptr[0])
			{
				case 'a'://add,append
				case 'p'://prepend
				case 's'://set, stats
				case 'r'://replace
				case 'c'://cas
				{
					return memcached_data_complete(ptr, data_len - i, &i_pkg_theory_len);
				}
				case 'b'://bget
				case 'g'://get, gets
				case 'i'://incr
				case 'd'://decr, delete
				case 'f'://flush
				{
					MEMCACHED_DEBUG("not update command\n");
					goto _send_up;
				}
				case 'q':
				{
					if(!memcmp(ptr, "quit", sizeof("quit") - 1))
					{
						return -1;//关闭链接
					}
					MEMCACHED_DEBUG("error command\n");
					goto _send_up;
					
				}
				default:
				{
					MEMCACHED_DEBUG("error command\n");
					goto _send_up;
				}
			}
		}
	}
	
	return 0;
	
_send_up:
	#ifdef MEMCACHED_DEBUG_ENABLE
//	memcpy(buf, (char*)data, sizeof(buf) - 1);
//	buf[sizeof(buf) - 1] = 0;	
//	MEMCACHED_DEBUG ("send %s to app", buf);
	#endif
	return data_len;	
}

int msg_header_len()
{
	return (int)sizeof(protocol_binary_request_no_extras);
}

}

