#ifndef ___MEMB_H_
#define ___MEMB_H_

#include "mblock.h"
#include "url.h"
typedef struct _MemBufferNode
{
    struct _MemBufferNode *next; /* link to next buffer node */
    int size;                    /* size of base */
    int pos;                     /* current read position */
    char base[1];
} MemBufferNode;
#define MEMBASESIZE (MIN_MBLOCK_SIZE - sizeof(MemBufferNode))

typedef struct _MemBuffer
{
    MemBufferNode *head; /* start buffer node pointer */
    MemBufferNode *tail; /* last buffer node pointer */
    MemBufferNode *cur;  /* current buffer node pointer */
    long total_size;
    MBlockList pool;
} MemBuffer;

extern void init_memb(MemBuffer *b);
extern void push_memb(MemBuffer *b, char *buff, long buff_size);
extern long read_memb(MemBuffer *b, char *buff, long buff_size);
extern long skip_read_memb(MemBuffer *b, long size);
extern void rewind_memb(MemBuffer *b);
extern void delete_memb(MemBuffer *b);
extern URL memb_open_stream(MemBuffer *b, int autodelete);

#endif /* ___MEMB_H_ */
