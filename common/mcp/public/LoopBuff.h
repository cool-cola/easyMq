#ifndef _LOOPBUFF_H_
#define _LOOPBUFF_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _LoopBuff LoopBuff;

LoopBuff*   Buff_create(int size);
void        Buff_free(LoopBuff*);
void        Buff_reset(LoopBuff*);

int      Buff_put(LoopBuff*, void* data, int size);
int      Buff_get(LoopBuff*, void* data, int size);
int      Buff_peek(LoopBuff*, void* data, int size);
int      Buff_erase(LoopBuff*, int size);

int      Buff_count(LoopBuff*);
int      Buff_freeCount(LoopBuff*);
int      Buff_dataCount(LoopBuff*);

char *  Buff_getWritePtr(LoopBuff *buff, int *len);
char *  Buff_getReadPtr(LoopBuff *buff, int *len);
void	Buff_getWritePtrs(LoopBuff* buff, char** p1, int* len1, char **p2, int *len2);
void	Buff_getReadPtrs(LoopBuff* buff, char **p1, int *len1, char **p2, int *len2);
int  Buff_stepWritePtr(LoopBuff *buff, int size);

#ifdef __cplusplus
}
#endif 
#endif 
