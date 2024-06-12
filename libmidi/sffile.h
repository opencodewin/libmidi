#ifndef SFFILE_H_DEF
#define SFFILE_H_DEF

/* chunk record header */
typedef struct _SFChunk
{
    char id[4];
    uint32 size;
} SFChunk;

/* generator record */
typedef struct _SFGenRec
{
    int16 oper;
    int16 amount;
} SFGenRec;

/* layered generators record */
typedef struct _SFGenLayer
{
    int nlists;
    SFGenRec *list;
} SFGenLayer;

/* header record */
typedef struct _SFHeader
{
    char name[20];
    uint16 bagNdx;
    /* layered stuff */
    int nlayers;
    SFGenLayer *layer;
} SFHeader;

/* preset header record */
typedef struct _SFPresetHdr
{
    SFHeader hdr;
    uint16 preset, bank;
    /*int32 lib, genre, morphology;*/ /* not used */
} SFPresetHdr;

/* instrument header record */
typedef struct _SFInstHdr
{
    SFHeader hdr;
} SFInstHdr;

/* sample info record */
typedef struct _SFSampleInfo
{
    char name[20];
    uint32 startsample, endsample;
    uint32 startloop, endloop;
    /* ver.2 additional info */
    uint32 samplerate;
    uint8 originalPitch;
    int8 pitchCorrection;
    uint16 samplelink;
    uint16 sampletype; /*1=mono, 2=right, 4=left, 8=linked, $8000=ROM*/
    /* optional info */
    int32 size;     /* sample size */
    int32 loopshot; /* short-shot loop size */
} SFSampleInfo;

/*----------------------------------------------------------------
 * soundfont file info record
 *----------------------------------------------------------------*/

typedef struct _SFInfo
{
    /* file name */
    char *sf_name;

    /* version of this file */
    uint16 version, minorversion;
    /* sample position (from origin) & total size (in bytes) */
    long samplepos;
    uint32 samplesize;

    /* raw INFO chunk list */
    long infopos;
	uint32 infosize;

    /* preset headers */
    int npresets;
    SFPresetHdr *preset;

    /* sample infos */
    int nsamples;
    SFSampleInfo *sample;

    /* instrument headers */
    int ninsts;
    SFInstHdr *inst;

} SFInfo;

/*----------------------------------------------------------------
 * functions
 *----------------------------------------------------------------*/

/* sffile.c */
extern int load_soundfont(SFInfo *sf, struct midi_file *fp);
extern void free_soundfont(SFInfo *sf);
extern void correct_samples(SFInfo *sf);

#endif
