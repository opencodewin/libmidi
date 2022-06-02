#ifndef ___AUDIO_CNV_H_
#define ___AUDIO_CNV_H_

extern const char u2c_table[256];
extern const char c2u_table[256];
extern const char a2c_table[256];
extern const char c2a_table[256];
extern const short u2s_table[256];
extern const char s2u_table[16384];
extern const short a2s_table[256];
extern const char s2a_table[16384];
extern const char u2a_table[256];
extern const char a2u_table[256];

/* U-law-8bit <---> linear-8bit */
#define AUDIO_U2C(X) (u2c_table[(unsigned char)(X)])
#define AUDIO_C2U(X) (c2u_table[(unsigned char)(X)])
extern void convert_u2c(const char *src, char *tar, int n);
extern void convert_c2u(const char *src, char *tar, int n);

/* A-law-8bit <---> linear-8bit */
#define AUDIO_A2C(X) (a2c_table[(unsigned char)(X)])
#define AUDIO_C2A(X) (c2a_table[(unsigned char)(X)])
extern void convert_a2c(const char *src, char *tar, int n);
extern void convert_c2a(const char *src, char *tar, int n);

/* U-law-8bit <---> linear-16bit */
#define AUDIO_U2S(X) (u2s_table[(unsigned char)(X)])
#define AUDIO_S2U(X) (s2u_table[((X) >> 2) & 0x3fff])
extern void convert_u2s(const char *src, short *tar, int n);
extern void convert_s2u(const short *src, char *tar, int n);

/* A-law-8bit <---> linear-16bit */
#define AUDIO_A2S(X) (a2s_table[(unsigned char)(X)])
#define AUDIO_S2A(X) (s2a_table[((X) >> 2) & 0x3fff])
extern void convert_a2s(const char *src, short *tar, int n);
extern void convert_s2a(const short *src, char *tar, int n);

/* U-law-8bit <---> A-law-8bit */
#define AUDIO_U2A(X) (u2a_table[(unsigned char)(X)])
#define AUDIO_A2U(X) (a2u_table[(unsigned char)(X)])
extern void convert_u2a(const char *src, char *tar, int n);
extern void convert_a2u(const char *src, char *tar, int n);

/*
 * U: U-law
 * A: A-law
 * C: signed 8-bit
 * S: signed 16-bit
 */

#endif /* ___AUDIO_CNV_H_ */
