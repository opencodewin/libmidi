#ifndef ___COMMON_H_
#define ___COMMON_H_
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include "sysdep.h"
#include "url.h"
#include "mblock.h"

/* 
    Filename extension, followed by command to run decompressor so that
    output is written to stdout. Terminate the list with a 0.

    Any file with a name ending in one of these strings will be run
    through the corresponding decompressor. If you don't like this
    behavior, you can undefine DECOMPRESSOR_LIST to disable automatic
    decompression entirely. */
//#define DECOMPRESSOR    1

#ifdef DECOMPRESSOR
#define DECOMPRESSOR_LIST              \
    {                                  \
        ".gz", "gunzip -c %s",         \
            ".xz", "xzcat %s",         \
            ".lzma", "lzcat %s",       \
            ".bz2", "bunzip2 -c %s",   \
            ".Z", "zcat %s",           \
            ".zip", "unzip -p %s",     \
            ".lha", "lha -pq %s",      \
            ".lzh", "lha -pq %s",      \
            ".shn", "shorten -x %s -", \
            0                          \
    }
#endif

/* Define GUS/patch converter. */
#define PATCH_CONVERTERS      \
    {                         \
        ".wav", "wav2pat %s", \
            0                 \
    }

/* 
    When a patch file can't be opened, one of these extensions is
    appended to the filename and the open is tried again.

    This is ignored for Windows, which uses only ".pat" (see the bottom
    of this file if you need to change this.) */
#define PATCH_EXT_LIST          \
    {                           \
        ".pat",                 \
            ".shn", ".pat.shn", \
            ".gz", ".pat.gz",   \
            ".bz2", ".pat.bz2", \
            0                   \
    }

/*****************************************************************************\
    section 1: some important definitions
\*****************************************************************************/
/*
    Anything below this shouldn't need to be changed unless you're porting
    to a new machine with other than 32-bit, big-endian words.
 */
/* change FRACTION_BITS above, not these */
#define INTEGER_BITS (32 - FRACTION_BITS)
#define INTEGER_MASK (0xFFFFFFFF << FRACTION_BITS)
#define FRACTION_MASK (~INTEGER_MASK)

/* This is enforced by some computations that must fit in an int */
#define MAX_CONTROL_RATIO 255

/* Vibrato and tremolo Choices of the Day */
#define SWEEP_TUNING 38
#define VIBRATO_AMPLITUDE_TUNING 1.0L
#define VIBRATO_RATE_TUNING 38
#define TREMOLO_AMPLITUDE_TUNING 1.0L
#define TREMOLO_RATE_TUNING 38

#define SWEEP_SHIFT 16
#define RATE_SHIFT 5

#define MODULATION_WHEEL_RATE (1.0 / 6.0)
/* #define MODULATION_WHEEL_RATE (midi_time_ratio/8.0) */
/* #define MODULATION_WHEEL_RATE (current_play_tempo/500000.0/32.0) */

#define VIBRATO_DEPTH_TUNING (1.0 / 4.0)

/* you cannot but use safe_malloc(). */
#define HAVE_SAFE_MALLOC 1
/* malloc's limit */
#define MAX_SAFE_MALLOC_SIZE (1 << 26) /* 64M, if midi file including millions voices, need increase memory size */

/* type of floating point number */
//typedef double FLOAT_T;
typedef float FLOAT_T;

/* These affect general volume */
#define GUARD_BITS 3
#define AMP_BITS (15 - GUARD_BITS)

#define MAX_AMPLIFICATION 800
#define MAX_CHANNELS 64
#define MAXMIDIPORT 16

#define VIBRATO_SAMPLE_INCREMENTS 32

/*  How many bits to use for the fractional part of sample positions.
    This affects tonal accuracy. The entire position counter must fit
    in 32 bits, so with FRACTION_BITS equal to 12, the maximum size of
    a sample is 1048576 samples (2 megabytes in memory). The GUS gets
    by with just 9 bits and a little help from its friends...
    "The GUS does not SUCK!!!" -- a happy user :) */
#define FRACTION_BITS 12

/* DEFAULT_VOICE is the polyphony number in boot-time.  This value is
    configurable by the command line option (-p) from 1 to until memory is
    allowed.  If your machine has much CPU power,  millions voices midi file 
    maybe need set to 16384 */
#define DEFAULT_VOICES 512

/*  The size of the internal buffer is 2^AUDIO_BUFFER_BITS samples.
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
#define AUDIO_BUFFER_BITS 12 /* Maxmum audio buffer size (2^bits) */

/* Audio buffer size has to be a power of two to allow DMA buffer
   fragments under the VoxWare (Linux & FreeBSD) audio driver */
#define AUDIO_BUFFER_SIZE (1 << AUDIO_BUFFER_BITS)

/* The number of samples to use for ramping out a dying note. Affects
   click removal. */
#define MAX_DIE_TIME 20

/* Acoustic Grand Piano seems to be the usual default instrument. */
#define DEFAULT_PROGRAM 0

/*  Specify drum channels (terminated with -1).
    10 is the standard percussion channel.
    Some files (notably C:\WINDOWS\CANYON.MID) think that 16 is one too.
    On the other hand, some files know that 16 is not a drum channel and
    try to play music on it. This is now a runtime option, so this isn't
    a critical choice anymore. */
#define DEFAULT_DRUMCHANNELS \
    {                        \
        10, -1               \
    }
/* #define DEFAULT_DRUMCHANNELS {10, 16, -1} */

/*  A somewhat arbitrary frequency range. The low end of this will
    sound terrible as no lowpass filtering is performed on most
    instruments before resampling. */
#define MIN_OUTPUT_RATE 4000
#define MAX_OUTPUT_RATE 400000

/* Master volume in percent. */
#define DEFAULT_AMPLIFICATION 100

/*  Default sampling rate, default polyphony, and maximum polyphony.
    All but the last can be overridden from the command line. */
#ifndef DEFAULT_RATE
#define DEFAULT_RATE 48000
#endif /* DEFAULT_RATE */

/*  The size of the internal buffer is 2^AUDIO_BUFFER_BITS samples.
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
#define AUDIO_BUFFER_BITS 12 /* Maxmum audio buffer size (2^bits) */

/*  1000 here will give a control ratio of 22:1 with 22 kHz output.
    Higher CONTROLS_PER_SECOND values allow more accurate rendering
    of envelopes and tremolo. The cost is CPU time. */
#define CONTROLS_PER_SECOND 1000

/*  Default resamplation algorighm.  Define as resample_XXX, where XXX is
    the algorithm name.  The following algorighms are available:
    cspline, gauss, newton, linear, none. */
#ifndef DEFAULT_RESAMPLATION
#define DEFAULT_RESAMPLATION resample_gauss
#endif

/*  Defining USE_DSP_EFFECT to refine chorus, delay, EQ and insertion effect.
    This is definitely worth the slight increase in CPU usage. */
#define USE_DSP_EFFECT

/* Greatly reduces popping due to large volume/pan changes.
 * This is definitely worth the slight increase in CPU usage. */
#define SMOOTH_MIXING

/*  Make envelopes twice as fast. Saves ~20% CPU time (notes decay
    faster) and sounds more like a GUS. There is now a command line
    option to toggle this as well. */
#define FAST_DECAY

/*  For some reason the sample volume is always set to maximum in all
    patch files. Define this for a crude adjustment that may help
    equalize instrument volumes. */
#define ADJUST_SAMPLE_VOLUMES

/*  On some machines (especially PCs without math coprocessors),
    looking up sine values in a table will be significantly faster than
    computing them on the fly. Uncomment this to use lookups. */
#define LOOKUP_SINE

/*  Shawn McHorse's resampling optimizations. These may not in fact be
    faster on your particular machine and compiler. You'll have to run
    a benchmark to find out. */
#define PRECALC_LOOPS

/*  If calling ldexp() is faster than a floating point multiplication
    on your machine/compiler/libm, uncomment this. It doesn't make much
    difference either way, but hey -- it was on the TODO list, so it
    got done. */
#define USE_LDEXP

/* Define the pre-resampling cache size.
 * This value is default. You can change the cache saze with
 * command line option.
 */
#define DEFAULT_CACHE_DATA_SIZE (4 * 1024 * 1024)

/* Where do you want to put temporary file into ?
 * If you are in UNIX, you can undefine this macro. If TMPDIR macro is
 * undefined, the value is used in environment variable `TMPDIR'.
 * If both macro and environment variable is not set, the directory is
 * set to /tmp.
 */
/* #define TMPDIR "/var/tmp" */

/* To use GS drumpart setting. */
#define GS_DRUMPART

/* Select output text code:
 * "AUTO"	- Auto conversion by `LANG' environment variable (UNIX only)
 * "ASCII"	- Convert unreadable characters to '.'(0x2e)
 * "NOCNV"	- No conversion
 * "EUC"	- EUC
 * "JIS"	- JIS
 * "SJIS"	- shift JIS
 */

#define OUTPUT_TEXT_CODE "ASCII"

/* Undefine if you don't use modulation wheel MIDI controls.
 * There is a command line option to enable/disable this mode.
 */
#define MODULATION_WHEEL_ALLOW

/* Undefine if you don't use portamento MIDI controls.
 * There is a command line option to enable/disable this mode.
 */
#define PORTAMENTO_ALLOW

/* Undefine if you don't use NRPN vibrato MIDI controls
 * There is a command line option to enable/disable this mode.
 */
#define NRPN_VIBRATO_ALLOW

/* Define if you want to use reverb / freeverb controls in defaults.
 * This mode needs high CPU power.
 * There is a command line option to enable/disable this mode.
 */
//#define REVERB_CONTROL_ALLOW
#define FREEVERB_CONTROL_ALLOW

/* Define if you want to use chorus controls in defaults.
 * This mode needs high CPU power.
 * There is a command line option to enable/disable this mode.
 */
#define CHORUS_CONTROL_ALLOW

/* Define if you want to use surround chorus in defaults.
 * This mode needs high CPU power.
 * There is a command line option to enable/disable this mode.
 */
#define SURROUND_CHORUS_ALLOW

/* Define if you want to use channel pressure.
 * Channel pressure effect is different in sequencers.
 * There is a command line option to enable/disable this mode.
 */
#define GM_CHANNEL_PRESSURE_ALLOW

/* Define if you want to use voice chamberlin / moog LPF.
 * This mode needs high CPU power.
 * There is a command line option to enable/disable this mode.
 */
#define VOICE_CHAMBERLIN_LPF_ALLOW
/* #define VOICE_MOOG_LPF_ALLOW */

/* Define if you want to use modulation envelope.
 * This mode needs high CPU power.
 * There is a command line option to enable/disable this mode.
 */
#define MODULATION_ENVELOPE_ALLOW

/* Define if you want to trace text meta event at playing.
 * There is a command line option to enable/disable this mode.
 */
#define ALWAYS_TRACE_TEXT_META_EVENT

/* Define if you want to allow overlapped voice.
 * There is a command line option to enable/disable this mode.
 */
#define OVERLAP_VOICE_ALLOW

/* Define if you want to allow temperament control.
 * There is a command line option to enable/disable this mode.
 */
#define TEMPER_CONTROL_ALLOW

/* Define if you want to allow auto reduce voice.
 */
//#define AUTO_REDUCE_VOICE_ALLOW

//#define HAVE_POPEN

#define DEFAULT_SOUNDFONT_ORDER 0

extern char *program_name, current_filename[];
extern const char *note_name[];
extern const unsigned char sfgm_bin[];
extern const size_t sfgm_size;

typedef struct
{
    char *path;
    void *next;
} PathList;

struct midi_file
{
    URL url;
    char *tmpname;
};

/* Noise modes for open_file */
#define OF_SILENT 0
#define OF_NORMAL 1
#define OF_VERBOSE 2

extern int load_font_from_cfg(char* path);
extern void add_to_pathlist(char *s);
extern void clean_up_pathlist(void);
extern int is_url_prefix(const char *name);
extern struct midi_file *open_file(char *name, int decompress, int noise_mode);
extern struct midi_file *open_file_r(char *name, int decompress, int noise_mode);
extern struct midi_file *open_with_mem(char *mem, int32 memlen, int noise_mode);
extern void close_file(struct midi_file *tf);
extern void skip(struct midi_file *tf, size_t len);
extern char *tf_gets(char *buff, int n, struct midi_file *tf);
#define tf_getc(tf) (url_getc((tf)->url))
extern long tf_read(void *buff, int32 size, int32 nitems, struct midi_file *tf);
extern long tf_seek(struct midi_file *tf, long offset, int whence);
extern long tf_tell(struct midi_file *tf);
extern int int_rand(int n); /* random [0..n-1] */
extern int check_file_extension(char *filename, char *ext, int decompress);

extern void *safe_malloc(size_t count);
extern void *safe_realloc(void *old_ptr, size_t new_size);
extern void *safe_large_malloc(size_t count);
extern char *safe_strdup(const char *s);
extern void free_ptr_list(void *ptr_list, int count);
extern int string_to_7bit_range(const char *s, int *start, int *end);
extern void randomize_string_list(char **strlist, int nstr);
extern int pathcmp(const char *path1, const char *path2, int ignore_case);
extern void sort_pathname(char **files, int nfiles);
extern int load_table(char *file);
extern char *pathsep_strrchr(const char *path);
extern char *pathsep_strchr(const char *path);
extern int str2mID(char *str);

/* code:
 * "EUC"	- Extended Unix Code
 * NULL		- Auto conversion
 * "JIS"	- Japanese Industrial Standard code
 * "SJIS"	- shift-JIS code
 */
extern void code_convert(char *in, char *out, int outsiz, char *in_code, char *out_code);

extern void safe_exit(int status);
extern char *mid2name(int mid);
extern const char * default_instrum_name_list[];
extern char *midi_version;
extern MBlockList tmpbuffer;
extern char *output_text_code;

static inline double get_current_calender_time(void)
{
    struct timeval tv;
    struct timezone dmy;

    gettimeofday(&tv, &dmy);

    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

#define imuldiv8(a, b) \
    (int32)(((int64)(a) * (int64)(b)) >> 8)

#define imuldiv16(a, b) \
    (int32)(((int64)(a) * (int64)(b)) >> 16)

#define imuldiv24(a, b) \
    (int32)(((int64)(a) * (int64)(b)) >> 24)

#define imuldiv28(a, b) \
    (int32)(((int64)(a) * (int64)(b)) >> 28)

static inline int32 signlong(int32 a)
{
    return ((a | 0x7fffffff) >> 30);
}

#endif /* ___COMMON_H_ */
