#include <sys/types.h>
#include <stdio.h>

static u_int32_t g_iBuildTable32 = 0;
static u_int32_t Table_CRC[256];
static u_int32_t  cnCRC_32 = 0x04C10DB7;

void BuildTable32(u_int32_t aPoly)
{
	if(g_iBuildTable32)	return;
	
	u_int32_t i, j;
	u_int32_t nData;
	u_int32_t nAccum;
	for ( i = 0; i < 256; i++ )
	{
		nData = ( u_int32_t )( i << 24 );
		nAccum = 0;
		for ( j = 0; j < 8; j++ )
		{
			if ( ( nData ^ nAccum ) & 0x80000000 )
				nAccum = ( nAccum << 1 ) ^ aPoly;
			else
				nAccum <<= 1;
			
			nData <<= 1;
		}
		Table_CRC[i] = nAccum;
	}

	g_iBuildTable32 = 1;
}
/*
CRC_32 具有极佳的分布特性和性能
*/
u_int32_t CRC_32(char *data, u_int32_t len)
{
	if(!g_iBuildTable32)
		BuildTable32( cnCRC_32 );

	unsigned char* pdata = (unsigned char*)data;
	u_int32_t i;
	u_int32_t nAccum = 0;
	for ( i = 0; i < len; i++ )
		nAccum = ( nAccum << 8 ) ^ Table_CRC[( nAccum >> 24 ) ^ *pdata++];
	
	return nAccum;
}

//------------------------------------------------------------------------------
#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
		  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const u_int16_t *) (d)))
#endif
		
#if !defined (get16bits)
#define get16bits(d) ((((u_int32_t)(((const u_int8_t *)(d))[1])) << 8)\
		                       +(u_int32_t)(((const u_int8_t *)(d))[0]) )
#endif

u_int32_t SuperFastHash(char *data, u_int32_t len) 
{
    u_int32_t hash = len, tmp;
    u_int32_t rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (u_int16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
            hash ^= hash << 16;
            hash ^= data[sizeof (u_int16_t)] << 18;
            hash += hash >> 11;
            break;
        case 2: hash += get16bits (data);
            hash ^= hash << 11;
            hash += hash >> 17;
            break;
        case 1: hash += *data;
            hash ^= hash << 10;
            hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}
//------------------------------------------------------------------------------
u_int32_t RSHash(char *data, u_int32_t len)
{
   u_int32_t b    = 378551;
   u_int32_t a    = 63689;
   u_int32_t hash = 0;

   for(u_int32_t i = 0; i < len; i++)
   {
      hash = hash * a + data[i];
      a    = a * b;
   }

   return hash;
}
//-----------------------------------------------------------------------------------
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

#if (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
     __BYTE_ORDER == __LITTLE_ENDIAN) || \
    (defined(i386) || defined(__i386__) || defined(__i486__) || \
     defined(__i586__) || defined(__i686__) || defined(vax) || defined(MIPSEL))
# define HASH_LITTLE_ENDIAN 1
# define HASH_BIG_ENDIAN 0
#elif (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && \
       __BYTE_ORDER == __BIG_ENDIAN) || \
      (defined(sparc) || defined(POWERPC) || defined(mc68000) || defined(sel))
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 1
#else
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 0
#endif

u_int32_t HashLittle(char* key, u_int32_t length)
{ 
  u_int32_t initval = 1734747458;
  u_int32_t a,b,c;                                          /* internal state */
  union { const void *ptr; size_t i; } u;     /* needed for Mac Powerbook G4 */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((u_int32_t)length) + initval;

  u.ptr = key;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const u_int32_t *k = (const u_int32_t *)key;         /* read 32-bit chunks */
    const u_int8_t  *k8;

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /* 
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff; a+=k[0]; break;
    case 5 : b+=k[1]&0xff; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff; break;
    case 2 : a+=k[0]&0xffff; break;
    case 1 : a+=k[0]&0xff; break;
    case 0 : return c;              /* zero length strings require no mixing */
    }
  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const u_int16_t *k = (const u_int16_t *)key;         /* read 16-bit chunks */
    const u_int8_t  *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12)
    {
      a += k[0] + (((u_int32_t)k[1])<<16);
      b += k[2] + (((u_int32_t)k[3])<<16);
      c += k[4] + (((u_int32_t)k[5])<<16);
      mix(a,b,c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const u_int8_t *)k;
    switch(length)
    {
    case 12: c+=k[4]+(((u_int32_t)k[5])<<16);
             b+=k[2]+(((u_int32_t)k[3])<<16);
             a+=k[0]+(((u_int32_t)k[1])<<16);
             break;
    case 11: c+=((u_int32_t)k8[10])<<16;     /* fall through */
    case 10: c+=k[4];
             b+=k[2]+(((u_int32_t)k[3])<<16);
             a+=k[0]+(((u_int32_t)k[1])<<16);
             break;
    case 9 : c+=k8[8];                      /* fall through */
    case 8 : b+=k[2]+(((u_int32_t)k[3])<<16);
             a+=k[0]+(((u_int32_t)k[1])<<16);
             break;
    case 7 : b+=((u_int32_t)k8[6])<<16;      /* fall through */
    case 6 : b+=k[2];
             a+=k[0]+(((u_int32_t)k[1])<<16);
             break;
    case 5 : b+=k8[4];                      /* fall through */
    case 4 : a+=k[0]+(((u_int32_t)k[1])<<16);
             break;
    case 3 : a+=((u_int32_t)k8[2])<<16;      /* fall through */
    case 2 : a+=k[0];
             break;
    case 1 : a+=k8[0];
             break;
    case 0 : return c;                     /* zero length requires no mixing */
    }

  } else {                        /* need to read the key one byte at a time */
    const u_int8_t *k = (const u_int8_t *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      a += ((u_int32_t)k[1])<<8;
      a += ((u_int32_t)k[2])<<16;
      a += ((u_int32_t)k[3])<<24;
      b += k[4];
      b += ((u_int32_t)k[5])<<8;
      b += ((u_int32_t)k[6])<<16;
      b += ((u_int32_t)k[7])<<24;
      c += k[8];
      c += ((u_int32_t)k[9])<<8;
      c += ((u_int32_t)k[10])<<16;
      c += ((u_int32_t)k[11])<<24;
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=((u_int32_t)k[11])<<24;
    case 11: c+=((u_int32_t)k[10])<<16;
    case 10: c+=((u_int32_t)k[9])<<8;
    case 9 : c+=k[8];
    case 8 : b+=((u_int32_t)k[7])<<24;
    case 7 : b+=((u_int32_t)k[6])<<16;
    case 6 : b+=((u_int32_t)k[5])<<8;
    case 5 : b+=k[4];
    case 4 : a+=((u_int32_t)k[3])<<24;
    case 3 : a+=((u_int32_t)k[2])<<16;
    case 2 : a+=((u_int32_t)k[1])<<8;
    case 1 : a+=k[0];
             break;
    case 0 : return c;
    }
  }

  final(a,b,c);
  return c;
}

u_int32_t HashBig(char* key, u_int32_t length)
{
  u_int32_t initval = 234343444;
  u_int32_t a,b,c;
  union { const void *ptr; size_t i; } u; /* to cast key to (size_t) happily */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((u_int32_t)length) + initval;

  u.ptr = key;
  if (HASH_BIG_ENDIAN && ((u.i & 0x3) == 0)) {
    const u_int32_t *k = (const u_int32_t *)key;         /* read 32-bit chunks */
    const u_int8_t  *k8;

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /* 
     * "k[2]<<8" actually reads beyond the end of the string, but
     * then shifts out the part it's not allowed to read.  Because the
     * string is aligned, the illegal read is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff00; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff0000; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff000000; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff00; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff0000; a+=k[0]; break;
    case 5 : b+=k[1]&0xff000000; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff00; break;
    case 2 : a+=k[0]&0xffff0000; break;
    case 1 : a+=k[0]&0xff000000; break;
    case 0 : return c;              /* zero length strings require no mixing */
    }
  } else {                        /* need to read the key one byte at a time */
    const u_int8_t *k = (const u_int8_t *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += ((u_int32_t)k[0])<<24;
      a += ((u_int32_t)k[1])<<16;
      a += ((u_int32_t)k[2])<<8;
      a += ((u_int32_t)k[3]);
      b += ((u_int32_t)k[4])<<24;
      b += ((u_int32_t)k[5])<<16;
      b += ((u_int32_t)k[6])<<8;
      b += ((u_int32_t)k[7]);
      c += ((u_int32_t)k[8])<<24;
      c += ((u_int32_t)k[9])<<16;
      c += ((u_int32_t)k[10])<<8;
      c += ((u_int32_t)k[11]);
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=k[11];
    case 11: c+=((u_int32_t)k[10])<<8;
    case 10: c+=((u_int32_t)k[9])<<16;
    case 9 : c+=((u_int32_t)k[8])<<24;
    case 8 : b+=k[7];
    case 7 : b+=((u_int32_t)k[6])<<8;
    case 6 : b+=((u_int32_t)k[5])<<16;
    case 5 : b+=((u_int32_t)k[4])<<24;
    case 4 : a+=k[3];
    case 3 : a+=((u_int32_t)k[2])<<8;
    case 2 : a+=((u_int32_t)k[1])<<16;
    case 1 : a+=((u_int32_t)k[0])<<24;
             break;
    case 0 : return c;
    }
  }

  final(a,b,c);
  return c;
}

//-----------------------------------------------------------------------------------
#define JHASH_GOLDEN_RATIO	0x9e3779b9

#ifndef CONFIG_IP_VS_TAB_BITS
#define CONFIG_IP_VS_TAB_BITS   12
#endif
/* make sure that IP_VS_CONN_TAB_BITS is located in [8, 20] */
#if CONFIG_IP_VS_TAB_BITS < 8
#define IP_VS_CONN_TAB_BITS	8
#endif
#if CONFIG_IP_VS_TAB_BITS > 20
#define IP_VS_CONN_TAB_BITS	20
#endif
#if 8 <= CONFIG_IP_VS_TAB_BITS && CONFIG_IP_VS_TAB_BITS <= 20
#define IP_VS_CONN_TAB_BITS	CONFIG_IP_VS_TAB_BITS
#endif
#define IP_VS_CONN_TAB_SIZE     (1 << IP_VS_CONN_TAB_BITS)
#define IP_VS_CONN_TAB_MASK     (IP_VS_CONN_TAB_SIZE - 1)

#define ip_vs_conn_rnd		434534534

#define __jhash_mix(a, b, c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

static inline u_int32_t jhash_3words(u_int32_t a, u_int32_t b, u_int32_t c, u_int32_t initval)
{
	a += JHASH_GOLDEN_RATIO;
	b += JHASH_GOLDEN_RATIO;
	c += initval;

	__jhash_mix(a, b, c);

	return c;
}

static unsigned int ip_vs_conn_hashkey(unsigned proto, u_int32_t addr, u_int16_t port)
{
	return jhash_3words(addr, port, proto, ip_vs_conn_rnd)
		& IP_VS_CONN_TAB_MASK;
}

u_int32_t Jhash3Words(char* key, u_int32_t length)
{
	u_int32_t addr = *(u_int32_t*)key;
	u_int16_t port = *(u_int16_t*)(key+4);
	return ip_vs_conn_hashkey(17,addr,port);
}
	
//-----------------------------------------------------------------------------------

/*
1024:
SuperFastHash 	307000/s
CRC_32 			140000/s
RSHash 			161000/s
HashLittle 		271000/s
HashBig 			176000/s
*/
#if 1

typedef u_int32_t (*HASH_FUNC)(char *data, u_int32_t len);
HASH_FUNC g_HashFuncArray[1024]=
{
	SuperFastHash,
	CRC_32,
	RSHash,
	HashLittle,
	HashBig,
	Jhash3Words,
	NULL
};

char g_HashNameArray[][128]=
{
	"SuperFastHash",
	"CRC_32",
	"RSHash",
	"HashLittle",
	"HashBig",
	"Jhash3Words",
	""
};

#define CNT	500000
#define KEY_LEN	6

#define BUCKET_NUM	5000000
#define KEY_NUM		1000000

#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <map>
using namespace std;

int main()
{
	//性能
	char *key = new char[KEY_LEN];
	for(int i=0; i<KEY_LEN;i++)
	{
		key[i] = (char)(i%256);
	}
		
	for(int i=0; i<1024;i++)
	{
		HASH_FUNC pHashFunc = g_HashFuncArray[i];
		if(!pHashFunc)
			break;

		char* pHashName = g_HashNameArray[i];

		timeval t1,t2;
		gettimeofday(&t1,NULL);
		for(int i=0;i<CNT;i++)
		{
			pHashFunc((char*)key,KEY_LEN);
		}
		gettimeofday(&t2,NULL);
		
		float span = (t2.tv_sec - t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/(float)1000;
		long long secnum = (long long)(CNT/(float)span) * (long long)1000;
		printf("%s %lld/s\n",pHashName,secnum);
	}

	printf("\n");
	
	//分布
	int *pBucket = new int[BUCKET_NUM];
	int now = time(0);
	for(int i=0; i<1024;i++)
	{
		HASH_FUNC pHashFunc = g_HashFuncArray[i];
		if(!pHashFunc)
			break;

		char* pHashName = g_HashNameArray[i];	
		printf("%s =>>\n",pHashName);

		srand(now);
		memset(pBucket,0,4*BUCKET_NUM);
		
		for(int j=0;j<KEY_NUM;j++)
		{
			//key
			unsigned int *piKey = (unsigned int*)key;
			for(int i=0; i<KEY_LEN/4;i++)
			{
				piKey[i] = rand();
			}
			for(int i=0; i<KEY_LEN%4;i++)
			{
				key[KEY_LEN-i] = rand()%256;
			}	

			//照顾ip+port类数据
			*(u_int16_t*)(key+4) = ((unsigned)rand())%65535;
		
			u_int32_t iIdx = pHashFunc((char*)key,KEY_LEN);
			iIdx = iIdx % BUCKET_NUM; 

			pBucket[iIdx]++;
		}

		map<int,int> bucket_map;
		for(int i=0;i<BUCKET_NUM;i++)
		{
			int node_num = pBucket[i];
			if(bucket_map.find(node_num) == bucket_map.end())
				bucket_map[node_num] = 1;
			else
				bucket_map[node_num]++;
		}

		printf("conflit:	num\n");
		map<int,int>::iterator it = bucket_map.begin();
		for(;it!=bucket_map.end();it++)
		{
			int node_num = it->first;
			int num = it->second;

			if(node_num)
				printf("%02d:	%d\n",node_num,num);
		}
	}
	
	return 0;
}
#endif


