#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "mblock.h"
#include "strtab.h"

void init_string_table(StringTable *stab)
{
    memset(stab, 0, sizeof(StringTable));
}

StringTableNode *put_string_table(StringTable *stab, char *str, int len)
{
    StringTableNode *p;

    p = new_segment(&stab->pool, sizeof(StringTableNode) + len + 1);
    if (p == NULL)
        return NULL;
    p->next = NULL;
    if (str != NULL)
    {
        memcpy(p->string, str, len);
        p->string[len] = '\0';
    }

    if (stab->head == NULL)
    {
        stab->head = stab->tail = p;
        stab->nstring = 1;
    }
    else
    {
        stab->nstring++;
        stab->tail = stab->tail->next = p;
    }
    return p;
}

char **make_string_array(StringTable *stab)
{
    char **table, *u;
    int i, n, s;
    StringTableNode *p;

    n = stab->nstring;
    if (n == 0)
        return NULL;
    if ((table = (char **)safe_malloc((n + 1) * sizeof(char *))) == NULL)
        return NULL;

    s = 0;
    for (p = stab->head; p; p = p->next)
        s += strlen(p->string) + 1;

    if ((u = (char *)safe_malloc(s)) == NULL)
    {
        free(table);
        return NULL;
    }

    for (i = 0, p = stab->head; p; i++, p = p->next)
    {
        int len;

        len = strlen(p->string) + 1;
        table[i] = u;
        memcpy(u, p->string, len);
        u += len;
    }
    table[i] = NULL;
    delete_string_table(stab);
    return table;
}

void delete_string_table(StringTable *stab)
{
    reuse_mblock(&stab->pool);
    init_string_table(stab);
}
