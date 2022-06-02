#ifndef ___STRTAB_H_
#define ___STRTAB_H_

#include "mblock.h"
typedef struct _StringTableNode
{
    struct _StringTableNode *next;
    char string[1]; /* variable length ('\0' terminated) */
} StringTableNode;

typedef struct _StringTable
{
    StringTableNode *head;
    StringTableNode *tail;
    uint16 nstring;
    MBlockList pool;
} StringTable;

extern void init_string_table(StringTable *stab);
extern StringTableNode *put_string_table(StringTable *stab, char *str, int len);
extern void delete_string_table(StringTable *stab);
extern char **make_string_array(StringTable *stab);

#endif /* ___STRTAB_H_ */
