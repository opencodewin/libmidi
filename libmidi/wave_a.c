#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <fcntl.h>
#include <ctype.h>
#include "common.h"
#include "output.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(char *buf, int32 bytes);
static int acntl(int request, void *arg);

/* export the playback mode */

#define dpm wave_play_mode

PlayMode dpm = {
    DEFAULT_RATE,
#ifdef LITTLE_ENDIAN
    /*PE_FLOAT | */PE_16BIT | PE_SIGNED,
#else
    /*PE_FLOAT | */PE_16BIT | PE_SIGNED | PE_BYTESWAP,
#endif
    PF_PCM_STREAM | PF_FILE_OUTPUT,
    -1,
    {0, 0, 0, 0, 0},
    "RIFF WAVE file",
    'w',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl};

#define UPDATE_HEADER_STEP (128 * 1024) /* 128KB */
static uint32 bytes_output, next_bytes;
static int already_warning_lseek;

/*************************************************************************/

static char *orig_RIFFheader =
    "RIFF"
    "\377\377\377\377"
    "WAVE"
    "fmt "
    "\020\000\000\000"
    "\001\000"
    /* 22: channels */ "\001\000"
    /* 24: frequency */ "xxxx"
    /* 28: bytes/second */ "xxxx"
    /* 32: bytes/sample */ "\004\000"
    /* 34: bits/sample */ "\020\000"
    "data"
    "\377\377\377\377";

/* We support follows WAVE format:
 * 8 bit unsigned pcm
 * 16 bit signed pcm (little endian)
 * 32 bit IEEE float
 * A-law
 * U-law
 */

/* Windows WAVE File Encoding Tags */
#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 0x01
#endif
#define WAVE_FORMAT_FLT 0x03
#define WAVE_FORMAT_ALAW 0x06
#define WAVE_FORMAT_MULAW 0x07

static int wav_output_open(const char *fname)
{
    int t;
    char RIFFheader[44];
    int fd;

    if (strcmp(fname, "-") == 0)
        fd = 1; /* data to stdout */
    else
    {
        /* Open the audio file */
        fd = open(fname, FILE_OUTPUT_MODE);
        if (fd < 0)
        {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", fname, strerror(errno));
            return -1;
        }
    }

    /* 
    Generate a (rather non-standard) RIFF header. We don't know yet
    what the block lengths will be. We'll fix that at close if this
    is a seekable file. */

    memcpy(RIFFheader, orig_RIFFheader, 44);

    if (dpm.encoding & PE_FLOAT)
        RIFFheader[20] = WAVE_FORMAT_FLT;
    else if (dpm.encoding & PE_ALAW)
        RIFFheader[20] = WAVE_FORMAT_ALAW;
    else if (dpm.encoding & PE_ULAW)
        RIFFheader[20] = WAVE_FORMAT_MULAW;
    else
        RIFFheader[20] = WAVE_FORMAT_PCM;

    if (dpm.encoding & PE_MONO)
        RIFFheader[22] = 1;
    else
        RIFFheader[22] = 2;

    *((int *)(RIFFheader + 24)) = LE_LONG(dpm.rate);

    t = dpm.rate;
    if (!(dpm.encoding & PE_MONO))
        t *= 2;

    if (dpm.encoding & PE_FLOAT)
        t *= 4;
    else if (dpm.encoding & PE_24BIT)
        t *= 3;
    else if (dpm.encoding & PE_16BIT)
        t *= 2;
    *((int *)(RIFFheader + 28)) = LE_LONG(t);

    if (dpm.encoding & PE_FLOAT)
        t = 4;
    else if (dpm.encoding & PE_16BIT)
        t = 2;
    else if (dpm.encoding & PE_24BIT)
        t = 3;
    else
        t = 1;
    RIFFheader[34] = t * 8;
    if (!(dpm.encoding & PE_MONO))
        t *= 2;
    RIFFheader[32] = t;

    if (std_write(fd, RIFFheader, 44) == -1)
    {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: write: %s", dpm.name, strerror(errno));
        close_output();
        return -1;
    }

    /* Reset the length counter */
    bytes_output = 0;
    next_bytes = bytes_output + UPDATE_HEADER_STEP;
    already_warning_lseek = 0;

    return fd;
}

static int auto_wav_output_open(const char *input_filename)
{
    char *output_filename;

    output_filename = create_auto_output_name(input_filename, "wav", NULL, 0);

    if (output_filename == NULL)
    {
        return -1;
    }
    if ((dpm.fd = wav_output_open(output_filename)) == -1)
    {
        free(output_filename);
        return -1;
    }
    if (dpm.name != NULL)
        free(dpm.name);
    dpm.name = output_filename;
    ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Output %s", dpm.name);
    return 0;
}

static int open_output(void)
{
    int include_enc, exclude_enc;

    include_enc = exclude_enc = 0;
    if (dpm.encoding & (PE_16BIT | PE_24BIT))
    {
#ifdef LITTLE_ENDIAN
        exclude_enc = PE_BYTESWAP;
#else
        include_enc = PE_BYTESWAP;
#endif /* LITTLE_ENDIAN */
        include_enc |= PE_SIGNED;
    }
    else if (!(dpm.encoding & (PE_ULAW | PE_ALAW)))
    {
        exclude_enc = PE_SIGNED;
    }

    dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);

    if (dpm.name == NULL)
    {
        dpm.flag |= PF_AUTO_SPLIT_FILE;
    }
    else
    {
        dpm.flag &= ~PF_AUTO_SPLIT_FILE;
        if ((dpm.fd = wav_output_open(dpm.name)) == -1)
            return -1;
    }
    return 0;
}

static int update_header(void)
{
    off_t save_point;
    int32 tmp;

    if (already_warning_lseek)
        return 0;

    save_point = lseek(dpm.fd, 0, SEEK_CUR);
    if (save_point == -1 || lseek(dpm.fd, 4, SEEK_SET) == -1)
    {
        ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Warning: %s: %s: Can't make valid header", dpm.name, strerror(errno));
        already_warning_lseek = 1;
        return 0;
    }

    tmp = LE_LONG(bytes_output + 44 - 8);
    if (std_write(dpm.fd, (char *)&tmp, 4) == -1)
    {
        lseek(dpm.fd, save_point, SEEK_SET);
        return -1;
    }
    lseek(dpm.fd, 40, SEEK_SET);
    tmp = LE_LONG(bytes_output);
    std_write(dpm.fd, (char *)&tmp, 4);

    lseek(dpm.fd, save_point, SEEK_SET);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG, "%s: Update RIFF WAVE header (size=%d)", dpm.name, bytes_output);

    return 0;
}

static int output_data(char *buf, int32 bytes)
{
    int n, i;

    if (dpm.fd == -1)
        return -1;

    if (dpm.encoding & PE_FLOAT)
    {
        int out_quant = bytes / 2;
        int out_bytes = out_quant * sizeof(float);
        float *fbuf = safe_malloc(out_bytes);
        for (i = 0; i < out_quant; i++)
        {
            fbuf[i] = ((short *)buf)[i] / 32768.0;
        }
        while (((n = std_write(dpm.fd, fbuf, out_bytes)) == -1) && errno == EINTR)
            ;
        free(fbuf);
        if (n == -1)
        {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", dpm.name, strerror(errno));
            return -1;
        }
        bytes_output += out_bytes;
    }
    else
    {
        while (((n = std_write(dpm.fd, buf, bytes)) == -1) && errno == EINTR)
        ;
        if (n == -1)
        {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", dpm.name, strerror(errno));
            return -1;
        }
        bytes_output += bytes;
    }

    
#if UPDATE_HEADER_STEP > 0
    if (bytes_output >= next_bytes)
    {
        if (update_header() == -1)
            return -1;
        next_bytes = bytes_output + UPDATE_HEADER_STEP;
    }
#endif /* UPDATE_HEADER_STEP */
    return n;
}

static void close_output(void)
{
    if (dpm.fd != 1 && /* We don't close stdout */
        dpm.fd != -1)
    {
        update_header();
        close(dpm.fd);
        dpm.fd = -1;
    }
}

static int acntl(int request, void *arg)
{
    switch (request)
    {
    case PM_REQ_PLAY_START:
        if (dpm.flag & PF_AUTO_SPLIT_FILE)
        {
            if ((NULL == current_file_info) || (NULL == current_file_info->filename))
                return auto_wav_output_open("Output.mid");
            return auto_wav_output_open(current_file_info->filename);
        }
        return 0;
    case PM_REQ_PLAY_END:
        if (dpm.flag & PF_AUTO_SPLIT_FILE)
            close_output();
        return 0;
    case PM_REQ_DISCARD:
        return 0;
    }
    return -1;
}
