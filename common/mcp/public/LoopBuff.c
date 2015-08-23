#include "LoopBuff.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

struct _LoopBuff 
{
	char* buf_;
	char* wptr_; //写指针
	char* rptr_; //读指针
	char* hptr_; //缓冲区的头指针
	char* tptr_; //缓冲区的尾指针
	int count_;
};
/*create a loop buf with 'size' bytes*/
LoopBuff* Buff_create(int size)
{
	LoopBuff* buff = (LoopBuff*)malloc(sizeof(LoopBuff));
	if(NULL == buff)
	{
		return NULL;
	}
	buff->count_ = size+1;
	buff->buf_ = (char*)malloc(buff->count_);
	if(NULL == buff->buf_)
	{
		free(buff);
		buff = NULL;
		return NULL;
	}
	buff->wptr_ = buff->rptr_ = buff->hptr_ = buff->buf_;
	buff->tptr_ = buff->hptr_ + buff->count_;
	return buff;
}
/*free the loop buf*/
void Buff_free(LoopBuff* buff) 
{
	if (buff)
	{
		if (buff->buf_!=NULL)
		{
			free(buff->buf_);
			buff->buf_ = 0;
		}
		free(buff);
		buff = 0;
	}
}
/*clear all data, add move to head*/
void Buff_reset(LoopBuff* buff)
{
	buff->hptr_ = buff->buf_;
	buff->tptr_ = buff->hptr_ + buff->count_;
	buff->wptr_ = buff->rptr_ = buff->hptr_;
}
/* try to put in 'ssize' bytes data, return actually putting in size*/
int Buff_put(LoopBuff*buff, void* data, int ssize) 
{
	char* buf = (char*)data;
	char* readptr	= buff->rptr_;
	int part		= buff->tptr_ - buff->wptr_;
	int size		= ssize; 
	
	if (buff->wptr_ >= readptr)
	{
		if (size > (buff->count_ - (buff->wptr_ - readptr) -1))
			size = buff->count_ - (buff->wptr_ - readptr) -1;
		if (part >= size)
		{				
			memcpy(buff->wptr_, buf, size);
			buff->wptr_ += size;
			return size;
		}
		else//part < size
		{
			memcpy(buff->wptr_, buf, part);
			buf += part;
			size -= part;
			if (size > (buff->rptr_ - buff->hptr_ - 1))
				size = buff->rptr_ - buff->hptr_ - 1;
			memcpy(buff->hptr_, buf, size);
			buff->wptr_ = buff->hptr_ + size;
			return part + size;
		}
	}
	else // wptr_ < readptr
	{
		if (size > (readptr - buff->wptr_ - 1))
			size = readptr - buff->wptr_ - 1;
		memcpy(buff->wptr_, buf, size);
		buff->wptr_ += size;
		return size;
	}
	
}
/* get 'size' bytes data, move read pointer*/
int Buff_get(LoopBuff*buff, void* data, int size) 
{
	char* buf = (char*)data;
	char* writeptr	= buff->wptr_;
	int part	= buff->tptr_ - buff->rptr_;
	
	if (writeptr >= buff->rptr_)
	{
		if (size > (writeptr - buff->rptr_))
			size = writeptr - buff->rptr_;
		memcpy(buf, buff->rptr_, size);
		buff->rptr_ += size;
		return size;
	}
	else// writeptr < rptr_
	{
		if (size > (buff->count_ - (buff->rptr_ - writeptr)))
			size = buff->count_ - (buff->rptr_ - writeptr);
		if (part >= size)
		{
			memcpy(buf, buff->rptr_, size);
			buff->rptr_ += size;
			return size;
		}
		else// part < size
		{
			memcpy(buf, buff->rptr_, part);
			buf += part;
			size -= part;
			if (size > (writeptr - buff->hptr_))
				size = writeptr - buff->hptr_;
			memcpy(buf, buff->hptr_, size);
			buff->rptr_ = buff->hptr_ + size;
			return part + size;
		}
	}
	
}
/* get 'size' bytes data, without move read pointer*/
int Buff_peek(LoopBuff*buff, void* data, int size) 
{
	char* buf = (char*)data;
	char* writeptr	= buff->wptr_;
	int part	= buff->tptr_ - buff->rptr_;
	
	if (writeptr >= buff->rptr_)
	{
		if (size > (writeptr - buff->rptr_))
			size = writeptr - buff->rptr_;
		memcpy(buf, buff->rptr_, size);
		return size;
	}
	else// writeptr < rptr_
	{
		if (size > (buff->count_ - (buff->rptr_ - writeptr)))
			size = buff->count_ - (buff->rptr_ - writeptr);
		if (part >= size)
		{
			memcpy(buf, buff->rptr_, size);
			return size;
		}
		else// part < size
		{
			memcpy(buf, buff->rptr_, part);
			buf += part;
			size -= part;
			if (size > (writeptr - buff->hptr_))
				size = writeptr - buff->hptr_;
			memcpy(buf, buff->hptr_, size);
			return part + size;
		}
	}
}
/*erase 'size' bytes data, move read pointer*/
int Buff_erase(LoopBuff*buff, int size) 
{
	char* writeptr	= buff->wptr_;
	int part	= buff->tptr_ - buff->rptr_;
	
	if (writeptr >= buff->rptr_)
	{
		if (size > (writeptr - buff->rptr_))
			size = writeptr - buff->rptr_;
		buff->rptr_ += size;
		return size;
	}
	else// writeptr < rptr_
	{
		if (size > (buff->count_ - (buff->rptr_ - writeptr)))
			size = buff->count_ - (buff->rptr_ - writeptr);
		if (part >= size)
		{
			buff->rptr_ += size;
			return size;
		}
		else// part < size
		{
			size -= part;
			if (size > (writeptr - buff->hptr_))
				size = writeptr - buff->hptr_;
			buff->rptr_ = buff->hptr_ + size;
			return part + size;
		}
	}
}
/* the capacity of the buf*/
int Buff_count(LoopBuff* buff)
{
	return buff->count_ - 1;
}
/* free count of the buf*/
int Buff_freeCount(LoopBuff* buff)
{
	char* writeptr	= buff->wptr_;
	char* readptr	= buff->rptr_;
	if (writeptr >= readptr)
		return buff->count_ - (writeptr - readptr) -1;
	else
		return (readptr - writeptr) -1;
}
/* data count of the buf*/
int Buff_dataCount(LoopBuff* buff)
{
	char* writeptr	= buff->wptr_;
	char* readptr	= buff->rptr_;
	if (writeptr >= readptr)
		return writeptr - readptr;
	else
		return buff->count_ - (readptr - writeptr);
}
/* get the first write pointer of the buf, and can put in how many bytes(len)
 * thus you can use it as raw buffer,ex. a[0], a[1], a[2].....	
 */
char *  Buff_getWritePtr(LoopBuff *buff, int *len)
{
	char *p1, *p2;
	int len1, len2;
	Buff_getWritePtrs(buff, &p1, &len1, &p2, &len2);
	if(p1){
		*len = len1;
		return p1;
	}
	if(p2){
		*len = len2;
		return p2;
	}
	*len = 0;
	return 0;
}
/* get the first read pointer of the buf, and can read how many bytes(len)
 * thus you can use it as raw buffer,ex. a[0], a[1], a[2].....	
 */
char *  Buff_getReadPtr(LoopBuff *buff, int *len)
{
	char *p1, *p2;
	int len1, len2;
	Buff_getReadPtrs(buff, &p1, &len1, &p2, &len2);
	if(p1){
		*len = len1;
		return p1;
	}
	if(p2){
		*len = len2;
		return p2;
	}
	*len = 0;
	return 0;
}
/*	get the write pointers of the buf, 2 pointers the max
 *	
 */
void	Buff_getWritePtrs(LoopBuff* buff, char** p1, int* len1, char **p2, int *len2)
{
	char* writeptr	= buff->wptr_;
	char *readptr = buff->rptr_;
	if(writeptr == readptr){//no data
		if(writeptr == buff->hptr_ || writeptr == buff->tptr_){
			*p1 = buff->hptr_;
			*len1 = buff->count_ - 1;
			*p2 = 0;
			*len2 = 0;
		}else{
			*p1 = writeptr;
			*len1 = buff->tptr_ - writeptr;
			*p2 = buff->hptr_;
			*len2 = readptr - buff->hptr_ - 1;
			if(*len2 == 0)
				*p2 = 0;
		}
		assert(*len1 + *len2 == buff->count_ -1);
	}else if(writeptr > readptr){
		*p1 = writeptr;
		*len1 = buff->tptr_ - writeptr;
		if(readptr == buff->hptr_)
			(*len1) --;
		if(*len1 < 0)
			*len1 = 0;
		if(*len1 == 0)
			*p1 = 0;
		
		*p2 = buff->hptr_;
		*len2 = readptr - buff->hptr_ - 1;
		if(*len2 < 0)
			*len2 = 0;
		if(*len2 == 0)
			*p2 = 0;
		assert(*len1 + *len2 == buff->count_ - 1 - (writeptr - readptr));
	}else{
		*p1 = writeptr;
		*len1 = readptr - writeptr -1;
		if(*len1 == 0)
			*p1 = 0;
		*p2 = 0;
		*len2 = 0;
		assert(*len1 + *len2 == readptr - writeptr - 1);
	}
}
/*
 *	get the read pointers , 2 pointers the max
 */
void  Buff_getReadPtrs(LoopBuff* buff, char **p1, int *len1, char **p2, int *len2)
{
	char* writeptr	= buff->wptr_;
	char *readptr = buff->rptr_;
	if(writeptr == readptr){
		*p1 = 0;
		*len1 = 0;
		*p2 = 0;
		*len2 = 0;
	}else if(writeptr > readptr){
		*p2 = 0;
		*len2 = 0;
		*len1 = writeptr - readptr;
		*p1 = readptr;
	}else{
		*len1 = buff->tptr_ - readptr;
		*p1 = readptr;
		if(*len1 == 0)
			*p1 = 0;
		*p2 = buff->hptr_;
		*len2 = writeptr - buff->hptr_;
		if(*len2 == 0)
			*p2 = 0;
	}
}
//使写指针前移
int  Buff_stepWritePtr(LoopBuff *buff, int ssize)
{
	char* readptr	= buff->rptr_;
	int part		= buff->tptr_ - buff->wptr_;
	int size		= ssize; 
	
	if (buff->wptr_ >= readptr)
	{
		if (size > (buff->count_ - (buff->wptr_ - readptr) -1))
			size = buff->count_ - (buff->wptr_ - readptr) -1;
		if (part >= size)
		{				
			buff->wptr_ += size;
			return size;
		}
		else//part < size
		{
			size -= part;
			if (size > (buff->rptr_ - buff->hptr_ - 1))
				size = buff->rptr_ - buff->hptr_ - 1;
			buff->wptr_ = buff->hptr_ + size;
			return part + size;
		}
	}
	else // wptr_ < readptr
	{
		if (size > (readptr - buff->wptr_ - 1))
			size = readptr - buff->wptr_ - 1;
		buff->wptr_ += size;
		return size;
	}
}












