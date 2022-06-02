#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "bitset.h"

#define CUTUP(n) (((n) + BIT_CHUNK_SIZE - 1) & ~(BIT_CHUNK_SIZE - 1))
#define CUTDOWN(n) ((n) & ~(BIT_CHUNK_SIZE - 1))

#define RFILLBITS(n) ((1u << (n)) - 1)

#define LFILLBITS(n) (RFILLBITS(n) << (BIT_CHUNK_SIZE - (n)))

static void print_uibits(unsigned int x)
{
    unsigned int mask;
    for (mask = (1u << (BIT_CHUNK_SIZE - 1)); mask; mask >>= 1)
        if (mask & x)
            putchar('1');
        else
            putchar('0');
}

void print_bitset(Bitset *bitset)
{
    int i, n;
    unsigned int mask;

    n = CUTDOWN(bitset->nbits) / BIT_CHUNK_SIZE;
    for (i = 0; i < n; i++)
    {
        print_uibits(bitset->bits[i]);
    }

    n = bitset->nbits - CUTDOWN(bitset->nbits);
    mask = (1u << (BIT_CHUNK_SIZE - 1));
    while (n--)
    {
        if (mask & bitset->bits[i])
            putchar('1');
        else
            putchar('0');
        mask >>= 1;
    }
}

void init_bitset(Bitset *bitset, int nbits)
{
    bitset->bits = (unsigned int *)
        safe_malloc((CUTUP(nbits) / BIT_CHUNK_SIZE) * sizeof(unsigned int));
    bitset->nbits = nbits;
    memset(bitset->bits, 0, (CUTUP(nbits) / BIT_CHUNK_SIZE) * sizeof(int));
}

void clear_bitset(Bitset *bitset, int start, int nbits)
{
    int i, j, sbitoff, ebitoff;
    unsigned int mask;

    if (nbits == 0 || start < 0 || start >= bitset->nbits)
        return;

    if (start + nbits > bitset->nbits)
        nbits = bitset->nbits - start;

    i = CUTDOWN(start);
    sbitoff = start - i;
    i /= BIT_CHUNK_SIZE;

    j = CUTDOWN(start + nbits - 1);
    ebitoff = start + nbits - j;
    /* ebitoff := [1 ... BIT_CHUNK_SIZE] */

    j /= BIT_CHUNK_SIZE;

    mask = LFILLBITS(sbitoff);

    if (i == j)
    {
        mask |= RFILLBITS(BIT_CHUNK_SIZE - ebitoff);
        bitset->bits[i] &= mask;
        return;
    }

    bitset->bits[i] &= mask;
    for (i++; i < j; i++)
        bitset->bits[i] = 0;
    mask = RFILLBITS(BIT_CHUNK_SIZE - ebitoff);
    bitset->bits[i] &= mask;
}

void set_bitset(Bitset *bitset, const unsigned int *bits, int start, int nbits)
{
    int i, j, lsbitoff, rsbitoff, ebitoff;
    unsigned int mask;

    if (nbits == 0 || start < 0 || start >= bitset->nbits)
        return;

    if (start + nbits > bitset->nbits)
        nbits = bitset->nbits - start;

    i = CUTDOWN(start);
    lsbitoff = start - i;
    rsbitoff = BIT_CHUNK_SIZE - lsbitoff;
    i /= BIT_CHUNK_SIZE;

    j = CUTDOWN(start + nbits - 1);
    ebitoff = start + nbits - j;
    /* ebitoff := [1 ... BIT_CHUNK_SIZE] */

    j /= BIT_CHUNK_SIZE;

    mask = LFILLBITS(lsbitoff);

    if (i == j)
    {
        mask |= RFILLBITS(BIT_CHUNK_SIZE - ebitoff);
        bitset->bits[i] = ((~mask & (*bits >> lsbitoff)) | (mask & bitset->bits[i]));
        return;
    }

    /* |1 2 3 4|1 2 3[...]
     *  \       \     \
     *   \       \     \
     * |1 2 3 4|1 2 3 4|1 2 3 4|...
     */
    bitset->bits[i] = ((~mask & (*bits >> lsbitoff)) | (mask & bitset->bits[i]));
    i++;
    bits++;
    for (; i < j; i++)
    {
        bitset->bits[i] = ((bits[-1] << rsbitoff) | (bits[0] >> lsbitoff));
        bits++;
    }
    mask = LFILLBITS(ebitoff);
    bitset->bits[i] = ((bits[-1] << rsbitoff) | ((mask & bits[0]) >> lsbitoff) | (~mask & bitset->bits[i]));
}

void get_bitset(const Bitset *bitset, unsigned int *bits, int start, int nbits)
{
    int i, j, lsbitoff, rsbitoff, ebitoff;

    memset(bits, 0, CUTUP(nbits) / 8);

    if (nbits == 0 || start < 0 || start >= bitset->nbits)
        return;

    if (start + nbits > bitset->nbits)
        nbits = bitset->nbits - start;

    i = CUTDOWN(start);
    lsbitoff = start - i;
    rsbitoff = BIT_CHUNK_SIZE - lsbitoff;
    i /= BIT_CHUNK_SIZE;

    j = CUTDOWN(start + nbits - 1);
    ebitoff = start + nbits - j;
    /* ebitoff := [1 ... BIT_CHUNK_SIZE] */

    j /= BIT_CHUNK_SIZE;

    if (i == j)
    {
        unsigned int mask;
        mask = LFILLBITS(lsbitoff) | RFILLBITS(BIT_CHUNK_SIZE - ebitoff);
        *bits = (~mask & bitset->bits[i]) << lsbitoff;
        return;
    }

    /* |1 2 3 4|1 2 3 4|1 2 3 4|...
     *     /       /   .   /
     *   /       /   .   /
     * |1 2 3 4|1 2 3[...]
     */

    for (; i < j; i++)
    {
        *bits = ((bitset->bits[i] << lsbitoff) | (bitset->bits[i + 1] >> rsbitoff));
        bits++;
    }

    if (ebitoff < lsbitoff)
        bits[-1] &= LFILLBITS(BIT_CHUNK_SIZE + ebitoff - lsbitoff);
    else
        *bits = (bitset->bits[i] << lsbitoff) & LFILLBITS(ebitoff - lsbitoff);
}

unsigned int has_bitset(const Bitset *bitset)
{
    int i, n;
    const unsigned int *p;

    n = CUTUP(bitset->nbits) / BIT_CHUNK_SIZE;
    p = bitset->bits;

    for (i = 0; i < n; i++)
        if (p[i])
            return 1;
    return 0;
}

int get_bitset1(Bitset *bitset, int n)
{
    int i;
    if (n < 0 || n >= bitset->nbits)
        return 0;
    i = BIT_CHUNK_SIZE - n - 1;
    return (bitset->bits[n / BIT_CHUNK_SIZE] & (1u << i)) >> i;
}

void set_bitset1(Bitset *bitset, int n, int bit)
{
    if (n < 0 || n >= bitset->nbits)
        return;
    if (bit)
        bitset->bits[n / BIT_CHUNK_SIZE] |=
            (1 << (BIT_CHUNK_SIZE - n - 1));
    else
        bitset->bits[n / BIT_CHUNK_SIZE] &=
            ~(1 << (BIT_CHUNK_SIZE - n - 1));
}

#if 0
void main(void)
{
    int i, j;
    Bitset b;
    unsigned int bits[3];

    init_bitset(&b, 96);

    b.bits[0] = 0x12345678;
    b.bits[1] = 0x9abcdef1;
    b.bits[2] = 0xaaaaaaaa;
    print_bitset(&b);

    for(i = 0; i <= 96; i++)
    {
	bits[0] = 0xffffffff;
	bits[1] = 0xffffffff;
	get_bitset(&b, bits, i, 60);
	print_uibits(bits[0]);putchar(',');
	print_uibits(bits[1]);
	putchar('\n');
    }
}
#endif
