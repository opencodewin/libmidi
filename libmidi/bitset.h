#ifndef ___BITSET_H_
#define ___BITSET_H_

typedef struct _Bitset
{
    int nbits;
    unsigned int *bits;
} Bitset;
#define BIT_CHUNK_SIZE ((unsigned int)(8 * sizeof(unsigned int)))

/*
 * Bitset
 */
extern void init_bitset(Bitset *bitset, int nbits);
extern void clear_bitset(Bitset *bitset, int start_bit, int nbits);
extern void get_bitset(const Bitset *bitset, unsigned int *bits_return, int start_bit, int nbits);
extern int get_bitset1(Bitset *bitset, int n);
extern void set_bitset(Bitset *bitset, const unsigned int *bits, int start_bit, int nbits);
extern void set_bitset1(Bitset *bitset, int n, int bit);
extern unsigned int has_bitset(const Bitset *bitset);
extern void print_bitset(Bitset *bitset);

#endif /* ___BITSET_H_ */
