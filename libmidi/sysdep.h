#ifndef SYSDEP_H_INCLUDED
#define SYSDEP_H_INCLUDED 1

#include <limits.h>

/* The size of the internal buffer is 2^AUDIO_BUFFER_BITS samples.
   This determines maximum number of samples ever computed in a row.

   For Linux and FreeBSD users:

   This also specifies the size of the buffer fragment.  A smaller
   fragment gives a faster response in interactive mode -- 10 or 11 is
   probably a good number. Unfortunately some sound cards emit a click
   when switching DMA buffers. If this happens to you, try increasing
   this number to reduce the frequency of the clicks.

   For other systems:

   You should probably use a larger number for improved performance.

*/
#ifndef DEFAULT_AUDIO_BUFFER_BITS
#define DEFAULT_AUDIO_BUFFER_BITS 11
#endif

#define SAMPLE_LENGTH_BITS 32

#ifndef NO_VOLATILE
#define VOLATILE_TOUCH(val) /* do nothing */
#define VOLATILE volatile
#else
extern int volatile_touch(void *dmy);
#define VOLATILE_TOUCH(val) volatile_touch(&(val))
#define VOLATILE
#endif /* NO_VOLATILE */

/* Byte order */
#ifdef WORDS_BIGENDIAN
#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#endif
#undef LITTLE_ENDIAN
#else
#undef BIG_ENDIAN
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif
#endif

/* integer type definitions: ISO C now knows a better way */
#include <stdint.h> // int types are defined here
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
#define MIDI_HAVE_INT64 1

/*  pointer size is not long in   WIN64 */
#if defined(_WIN32)
typedef long long ptr_size_t;
typedef unsigned long long u_ptr_size_t;
#else
typedef long ptr_size_t;
typedef unsigned long u_ptr_size_t;
#endif

/* Instrument files are little-endian, MIDI files big-endian, so we
   need to do some conversions. */
#define XCHG_SHORT(x) ((((x)&0xFF) << 8) | (((x) >> 8) & 0xFF))
#define XCHG_LONG(x) ((((x)&0xFF) << 24) |      \
                    (((x)&0xFF00) << 8) |       \
                    (((x)&0xFF0000) >> 8) |     \
                    (((x) >> 24) & 0xFF))

#ifdef LITTLE_ENDIAN
#define LE_SHORT(x) (x)
#define LE_LONG(x) (x)
#define BE_SHORT(x) XCHG_SHORT(x)
#define BE_LONG(x) XCHG_LONG(x)
#else /* BIG_ENDIAN */
#define BE_SHORT(x) (x)
#define BE_LONG(x) (x)
#define LE_SHORT(x) XCHG_SHORT(x)
#define LE_LONG(x) XCHG_LONG(x)
#endif /* LITTLE_ENDIAN */

#if MAX_CHANNELS <= 32
typedef struct _ChannelBitMask
{
    uint32 b; /* 32-bit bitvector */
} ChannelBitMask;
#define CLEAR_CHANNELMASK(bits) ((bits).b = 0)
#define FILL_CHANNELMASK(bits) ((bits).b = ~0)
#define IS_SET_CHANNELMASK(bits, c) ((bits).b & (1u << (c)))
#define SET_CHANNELMASK(bits, c) ((bits).b |= (1u << (c)))
#define UNSET_CHANNELMASK(bits, c) ((bits).b &= ~(1u << (c)))
#define TOGGLE_CHANNELMASK(bits, c) ((bits).b ^= (1u << (c)))
#define COPY_CHANNELMASK(dest, src) ((dest).b = (src).b)
#define REVERSE_CHANNELMASK(bits) ((bits).b = ~(bits).b)
#define COMPARE_CHANNELMASK(bitsA, bitsB) ((bitsA).b == (bitsB).b)
#else
typedef struct _ChannelBitMask
{
    uint32 b[8]; /* 256-bit bitvector */
} ChannelBitMask;
#define CLEAR_CHANNELMASK(bits) \
    memset((bits).b, 0, sizeof(ChannelBitMask))
#define FILL_CHANNELMASK(bits) \
    memset((bits).b, 0xFF, sizeof(ChannelBitMask))
#define IS_SET_CHANNELMASK(bits, c) \
    ((bits).b[((c) >> 5) & 0x7] & (1u << ((c)&0x1F)))
#define SET_CHANNELMASK(bits, c) \
    ((bits).b[((c) >> 5) & 0x7] |= (1u << ((c)&0x1F)))
#define UNSET_CHANNELMASK(bits, c) \
    ((bits).b[((c) >> 5) & 0x7] &= ~(1u << ((c)&0x1F)))
#define TOGGLE_CHANNELMASK(bits, c) \
    ((bits).b[((c) >> 5) & 0x7] ^= (1u << ((c)&0x1F)))
#define COPY_CHANNELMASK(dest, src) \
    memcpy(&(dest), &(src), sizeof(ChannelBitMask))
#define REVERSE_CHANNELMASK(bits) \
    ((bits).b[((c) >> 5) & 0x7] = ~(bits).b[((c) >> 5) & 0x7])
#define COMPARE_CHANNELMASK(bitsA, bitsB) \
    (memcmp((bitsA).b, (bitsB).b, sizeof((bitsA).b)) == 0)
#endif

typedef int16 sample_t;
typedef int32 final_volume_t;
#define FINAL_VOLUME(v) (v)
#define MAX_AMP_VALUE ((1 << (AMP_BITS + 1)) - 1)
#define MIN_AMP_VALUE (MAX_AMP_VALUE >> 9)

#if SAMPLE_LENGTH_BITS > 32
#if MIDI_HAVE_INT64
typedef int64 splen_t;
#define SPLEN_T_MAX (splen_t)((((uint64)1) << 63) - 1)
#else /* MIDI_HAVE_INT64 */
typedef uint32 splen_t;
#define SPLEN_T_MAX (splen_t)((uint32)0xFFFFFFFF)
#endif /* MIDI_HAVE_INT64 */
#elif SAMPLE_LENGTH_BITS == 32
typedef uint32 splen_t;
#define SPLEN_T_MAX (splen_t)((uint32)0xFFFFFFFF)
#else /* SAMPLE_LENGTH_BITS */
typedef int32 splen_t;
#define SPLEN_T_MAX (splen_t)((((uint32)1) << 31) - 1)
#endif /* SAMPLE_LENGTH_BITS */

#ifdef USE_LDEXP
#define TIM_FSCALE(a, b) ldexp((double)(a), (b))
#define TIM_FSCALENEG(a, b) ldexp((double)(a), -(b))
#include <math.h>
#else
#define TIM_FSCALE(a, b) ((a) * (double)(1 << (b)))
#define TIM_FSCALENEG(a, b) ((a) * (1.0 / (double)(1 << (b))))
#endif

/* The path separator (D.M.) */
/* Windows: "\"
 * Cygwin:  both "\" and "/"
 * Macintosh: ":"
 * Unix: "/"
 */
#define PATH_SEP '/'
#define PATH_STRING "/"

#ifdef PATH_SEP2
#define IS_PATH_SEP(c) ((c) == PATH_SEP || (c) == PATH_SEP2)
#else
#define IS_PATH_SEP(c) ((c) == PATH_SEP)
#endif

/* new line code */
#ifndef NLS
#define NLS "\n"
#endif /* NLS */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif /* M_PI */

#undef MAIL_NAME

#define SAFE_CONVERT_LENGTH(len) (6 * (len) + 1)

#ifndef HAVE_POPEN
#undef DECOMPRESSOR_LIST
#undef PATCH_CONVERTERS
#endif

#endif /* SYSDEP_H_INCUDED */
