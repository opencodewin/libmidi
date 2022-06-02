#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <SDL.h>
#include <SDL_thread.h>
#include "common.h"
#include "output.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"
#include "miditrace.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(char *buf, int32 in_bytes);
static int acntl(int request, void *arg);
static float output_volume = 1.0;
static int buf_using_num = 0;
/* export the playback mode */

/*********************************************************************/
#define dpm sdl_play_mode

PlayMode dpm = {
    DEFAULT_RATE, PE_16BIT | PE_SIGNED, PF_PCM_STREAM | PF_CAN_TRACE | PF_BUFF_FRAGM_OPT, -1, {0}, /* default: get all the buffer fragments you can */
    "SDL pcm device",
    's',
    "",
    open_output,
    close_output,
    output_data,
    acntl};

#define FailWithAction(cond, action, handler) \
    if (cond)                                 \
    {                                         \
        {                                     \
            action;                           \
        }                                     \
        goto handler;                         \
    }

/*********************************************************************/

#ifdef AUTO_REDUCE_VOICE_ALLOW
#define BUFNUM 4
#else
#define BUFNUM 16
#endif
#define BUFLEN 2048
typedef struct
{
    int soundPlaying;
    SDL_AudioDeviceID m_audio_dev;
    int mBytesPerPacket;
    // info about the default device
    char buffer[BUFNUM][BUFLEN]; // 1 buffer size = 2048bytes
    unsigned short buffer_len[BUFNUM];
    volatile int readBuf, writeBuf;
    int samples, get_samples;
} appGlobals, *appGlobalsPtr, **appGlobalsHandle;

static appGlobals globals = {0};

static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    Uint8 * stream_data = stream;
    int trans_len = 0;
    while (len > 0)
    {
        if (buf_using_num == 0)
        { 
            // empty
            memset(stream_data, 0, len);
            //globals.soundPlaying = 0;
            return;
        }
        trans_len = globals.buffer_len[globals.readBuf];
        if (trans_len > len)
        {
            break;
        }
        if (output_volume == 1.0)
        {
            memcpy(stream_data, globals.buffer[globals.readBuf], trans_len); // move data into output data buffer
        }
        else
        {
            float *src = (float *)globals.buffer[globals.readBuf];
            float *dst = (float *)stream_data;
            int i, quant = trans_len / sizeof(float);
            for (i = 0; i < quant; i++)
            {
                *dst++ = (*src++) * output_volume;
            }
        }
        globals.samples += trans_len / globals.mBytesPerPacket;
        globals.readBuf ++;
        if (globals.readBuf >= BUFNUM) globals.readBuf = 0;
        buf_using_num--;
        len -= trans_len;
        stream_data += trans_len;
    }
}

/*********************************************************************/
static void init_variable()
{
    globals.samples = globals.get_samples = 0;
    globals.readBuf = globals.writeBuf = 0;
    globals.soundPlaying = 0;
    buf_using_num = 0;
    dpm.fd = globals.m_audio_dev;
}

/*********************************************************************/
/*return value == 0 sucess
 *             == -1 fails
 */
static int open_output(void)
{
    SDL_AudioSpec wanted_spec, spec;
    wanted_spec.channels = dpm.encoding & PE_MONO ? 1 : 2;
    wanted_spec.freq = dpm.rate;
    wanted_spec.format = AUDIO_F32SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = BUFLEN / sizeof(float) *  wanted_spec.channels;
    wanted_spec.callback = sdl_audio_callback;
    wanted_spec.userdata = NULL;
    globals.m_audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, 0 /*SDL_AUDIO_ALLOW_ANY_CHANGE*/);
    if (globals.m_audio_dev)
    {
        SDL_PauseAudioDevice(globals.m_audio_dev, 1);
        globals.mBytesPerPacket = sizeof(float) * wanted_spec.channels;
        init_variable();
        return 0;
    }
    
    return -1;
}

/*********************************************************************/
static int output_data(char *buf, int32 in_bytes)
{
    int out_bytes;
    int inBytesPerQuant, max_quant, max_outbytes, out_quant, i;
    float maxLevel;

    if (!globals.m_audio_dev)
        return -1;
    // quant  : 1 value
    // packet : 1 pair of quant(stereo data)

    if (dpm.encoding & PE_16BIT)
    {
        inBytesPerQuant = 2;
        maxLevel = 32768.0;
    }
    else if (dpm.encoding & PE_24BIT)
    {
        inBytesPerQuant = 3;
        maxLevel = 8388608.0;
    }
    else
    {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Sorry, not support 8bit sound.");
        exit(1);
    }

    max_outbytes = BUFLEN;
    max_quant = max_outbytes / sizeof(float);
    if (globals.soundPlaying == 0)
    {
        SDL_PauseAudioDevice(globals.m_audio_dev, 0);
        // start playing sound through the device
        globals.soundPlaying = 1; // set the playing status global to true
    }

redo:
    while (buf_using_num == BUFNUM)
    {
        // queue full
        usleep(10000); // 0.01sec
    }

    out_quant = in_bytes / inBytesPerQuant;
    out_bytes = out_quant * sizeof(float);
    if (out_bytes > max_outbytes)
    {
        out_bytes = max_outbytes;
    }
    out_quant = out_bytes / sizeof(float);
    out_quant &= 0xfffffffe; // trunc to eaven number for stereo
    out_bytes = out_quant * sizeof(float);

    switch (inBytesPerQuant)
    {

    case 2:
        for (i = 0; i < out_quant; i++)
        {
            ((float *)(globals.buffer[globals.writeBuf]))[i] =
                ((short *)buf)[i] / maxLevel;
        }
        break;

    case 3:
        for (i = 0; i < out_quant; i++)
        {
            ((float *)(globals.buffer[globals.writeBuf]))[i] =
                (*(int32 *)&buf[i * 3] >> 8) / maxLevel;
        }
        break;
    }

    globals.buffer_len[globals.writeBuf] = out_bytes;

    globals.writeBuf ++;
    if (globals.writeBuf >= BUFNUM) globals.writeBuf = 0;
    in_bytes -= out_quant * inBytesPerQuant;
    buf += out_quant * inBytesPerQuant;
    buf_using_num++;
    globals.get_samples += out_bytes / globals.mBytesPerPacket;

    if (in_bytes)
    {
        goto redo;
    }

Bail:
    return 0;
}

/*********************************************************************/
static void close_output(void)
{
    if (globals.m_audio_dev)
    {
        SDL_ClearQueuedAudio(globals.m_audio_dev);
        SDL_CloseAudioDevice(globals.m_audio_dev);
    }
    globals.soundPlaying = 0;
    dpm.fd = -1;
}

/*********************************************************************/
static int acntl(int request, void *arg)
{
    switch (request)
    {
    case PM_REQ_GETFRAGSIZ:
        if (dpm.encoding & PE_24BIT)
        {
            *((int *)arg) = 3072;
        }
        else
        {
            *((int *)arg) = BUFLEN;
        }
        return 0;

    case PM_REQ_GETSAMPLES:
        *((int *)arg) = globals.samples;
        return 0;

    case PM_REQ_DISCARD:
        SDL_PauseAudioDevice(globals.m_audio_dev, 1);
        SDL_ClearQueuedAudio(globals.m_audio_dev);
        init_variable();
        return 0;

    case PM_REQ_PLAY_START:
        init_variable();
        return 0;

    case PM_REQ_FLUSH:
    case PM_REQ_OUTPUT_FINISH:
        while (buf_using_num)
        {
            trace_loop();
            usleep(1000);
        }
        init_variable();
        return 0;
    case PM_REQ_GETFILLED:
        *((int *)arg) = buf_using_num * BUFLEN;
        return 0;
    default:
        break;
    }
    return -1;
}
