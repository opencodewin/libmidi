#ifndef ___MBLOCK_H_
#define ___MBLOCK_H_

/* Memory block for decreasing malloc
 *
 * +------+    +------+             +-------+
 * |BLOCK1|--->|BLOCK2|---> ... --->|BLOCK N|---> NULL
 * +------+    +------+             +-------+
 *
 *
 * BLOCK:
 * +-----------------------+
 * | memory 1              |
 * |                       |
 * +-----------------------+
 * | memory 2              |
 * +-----------------------+
 * | memory 3              |
 * |                       |
 * |                       |
 * +-----------------------+
 * | unused ...            |
 * +-----------------------+
 */

#define MIN_MBLOCK_SIZE 8192

typedef struct _MBlockNode
{
    size_t block_size;
    size_t offset;
    struct _MBlockNode *next;
#ifndef MBLOCK_NOPAD
    void *pad;
#endif /* MBLOCK_NOPAD */
    char buffer[1];
} MBlockNode;

typedef struct _MBlockList
{
    MBlockNode *first;
    size_t allocated;
} MBlockList;

extern void init_mblock(MBlockList *mblock);
extern void *new_segment(MBlockList *mblock, size_t nbytes);
extern void reuse_mblock(MBlockList *mblock);
extern char *strdup_mblock(MBlockList *mblock, const char *str);
extern int free_global_mblock(void);

#endif /* ___MBLOCK_H_ */
