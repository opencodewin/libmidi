#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "mblock.h"

static MBlockNode *free_mblock_list = NULL;
#define ADDRALIGN 8
/* #define DEBUG */

void init_mblock(MBlockList *mblock)
{
    mblock->first = NULL;
    mblock->allocated = 0;
}

static MBlockNode *new_mblock_node(size_t n)
{
    MBlockNode *p;

    if (n > MIN_MBLOCK_SIZE)
    {
        if ((p = (MBlockNode *)safe_malloc(n + sizeof(MBlockNode))) == NULL)
            return NULL;
        p->block_size = n;
    }
    else if (free_mblock_list == NULL)
    {
        if ((p = (MBlockNode *)safe_malloc(sizeof(MBlockNode) + MIN_MBLOCK_SIZE)) == NULL)
            return NULL;
        p->block_size = MIN_MBLOCK_SIZE;
    }
    else
    {
        p = free_mblock_list;
        free_mblock_list = free_mblock_list->next;
    }

    p->offset = 0;
    p->next = NULL;

    return p;
}

static int enough_block_memory(MBlockList *mblock, size_t n)
{
    size_t newoffset;

    if (mblock->first == NULL)
        return 0;

    newoffset = mblock->first->offset + n;

    if (newoffset < mblock->first->offset) /* exceed representable in size_t */
        return 0;

    if (newoffset > mblock->first->block_size)
        return 0;

    return 1;
}

void *new_segment(MBlockList *mblock, size_t nbytes)
{
    MBlockNode *p;
    void *addr;

    /* round up to ADDRALIGN */
    nbytes = ((nbytes + ADDRALIGN - 1) & ~(ADDRALIGN - 1));
    if (!enough_block_memory(mblock, nbytes))
    {
        p = new_mblock_node(nbytes);
        p->next = mblock->first;
        mblock->first = p;
        mblock->allocated += p->block_size;
    }
    else
        p = mblock->first;

    addr = (void *)(p->buffer + p->offset);
    p->offset += nbytes;

#ifdef DEBUG
    if (((unsigned long)addr) & (ADDRALIGN - 1))
    {
        fprintf(stderr, "Bad address: 0x%x\n", addr);
        exit(1);
    }
#endif /* DEBUG */

    return addr;
}

static void reuse_mblock1(MBlockNode *p)
{
    if (p->block_size > MIN_MBLOCK_SIZE)
        free(p);
    else /* p->block_size <= MIN_MBLOCK_SIZE */
    {
        p->next = free_mblock_list;
#ifdef DEBUG
        memset(p->buffer, 0xa5, p->block_size);
#endif
        free_mblock_list = p;
    }
}

void reuse_mblock(MBlockList *mblock)
{
    MBlockNode *p;

    if ((p = mblock->first) == NULL)
        return; /* There is nothing to collect memory */

    while (p)
    {
        MBlockNode *tmp;

        tmp = p;
        p = p->next;
        reuse_mblock1(tmp);
    }
    init_mblock(mblock);
}

char *strdup_mblock(MBlockList *mblock, const char *str)
{
    int len;
    char *p;

    len = strlen(str);
    p = (char *)new_segment(mblock, len + 1); /* for '\0' */
    memcpy(p, str, len + 1);
    return p;
}

int free_global_mblock(void)
{
    int cnt;

    cnt = 0;
    while (free_mblock_list)
    {
        MBlockNode *tmp;

        tmp = free_mblock_list;
        free_mblock_list = free_mblock_list->next;
        free(tmp);
        cnt++;
    }
    return cnt;
}
