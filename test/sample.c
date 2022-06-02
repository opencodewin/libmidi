#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "miditrace.h"
#include "reverb.h"
#include "resample.h"
#include "recache.h"
#include "strtab.h"
#include "aq.h"
#include "mix.h"
#include "quantity.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <SDL.h>

extern struct URL_module URL_module_file;
extern struct URL_module URL_module_dir;
struct URL_module *url_module_list[] =
{
    &URL_module_file,
    NULL
};

void print_help(char * prog)
{
    printf("Usage:\n");
    printf("\t%s [-f soundfont] midi_file\n", prog);
}

int main(int argc, char ** argv)
{
    int ch;
    opterr = 0;
    int nfiles = 0;
    char **files = NULL;
    char *sf_file = NULL;
    const char *optstring = "f:h";
    while((ch = getopt(argc, argv, optstring)) != -1)
    {
        switch(ch)
        {
            case 'f': sf_file = optarg; break;
            case 'h': print_help(argv[0]); return 0;
            default: break;
        }
    }
    nfiles = argc - optind;
    files = argv + optind;
    
    play_mode = play_mode_list[3];
    ctl = ctl_list[0];
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);
    /* init play */
    for (int i = 0; i < MAX_CHANNELS; i++) memset(&(channel[i]), 0, sizeof(Channel));
    CLEAR_CHANNELMASK(quietchannels);
    CLEAR_CHANNELMASK(default_drumchannels);
    static int drums[] = DEFAULT_DRUMCHANNELS;
    for (int i = 0; drums[i] > 0; i++)
        SET_CHANNELMASK(default_drumchannels, drums[i] - 1);
#if MAX_CHANNELS > 16
    for (int i = 16; i < MAX_CHANNELS; i++)
    {
        if (IS_SET_CHANNELMASK(default_drumchannels, i & 0xF))
            SET_CHANNELMASK(default_drumchannels, i);
    }
#endif

    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        default_program[i] = DEFAULT_PROGRAM;
        memset(channel[i].drums, 0, sizeof(channel[i].drums));
    }

    for (int i = 0; url_module_list[i]; i++) url_add_module(url_module_list[i]);
    init_freq_table();
    init_freq_table_tuning();
    init_freq_table_pytha();
    init_freq_table_meantone();
    init_freq_table_pureint();
    init_freq_table_user();
    init_bend_fine();
    init_bend_coarse();
    init_tables();
    init_gm2_pan_table();
    init_attack_vol_table();
    init_sb_vol_table();
    init_modenv_vol_table();
    init_def_vol_table();
    init_gs_vol_table();
    init_perceived_vol_table();
    init_gm2_vol_table();
    for (int i = 0; i < NSPECIAL_PATCH; i++) special_patch[i] = NULL;
    init_midi_trace();
    int_rand(-1); /* initialize random seed */
    int_rand(42); /* the 1st number generated is not very random */

    initialize_resampler_coeffs();
    /* Allocate voice[] */
    voice = (Voice *)safe_realloc(voice, max_voices * sizeof(Voice));
    memset(voice, 0, max_voices * sizeof(Voice));
    /* Set play mode parameters */
    if (play_mode->rate == 0)
        play_mode->rate = DEFAULT_RATE;

    /* save defaults */
    COPY_CHANNELMASK(drumchannels, default_drumchannels);
    COPY_CHANNELMASK(drumchannel_mask, default_drumchannel_mask);

    if (ctl->open(0, 0))
    {
        fprintf(stderr, "Couldn't open %s (`%c')" NLS, ctl->id_name, ctl->id_character);
        SDL_Quit();
        return 3;
    }

    /* Open output device */
    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
            "Open output: %c, %s",
            play_mode->id_character,
            play_mode->id_name);

    if (play_mode->flag & PF_PCM_STREAM)
    {
        play_mode->extra_param[1] = aq_calc_fragsize();
        ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
                "requesting fragment size: %d",
                play_mode->extra_param[1]);
    }

    if (play_mode->open_output() < 0)
    {
        ctl->cmsg(CMSG_FATAL, VERB_NORMAL,
                "Couldn't open %s (`%c')",
                play_mode->id_name, play_mode->id_character);
        ctl->close();
        SDL_Quit();
        return 2;
    }

    if (!control_ratio)
    {
        control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
        if (control_ratio < 1)
            control_ratio = 1;
        else if (control_ratio > MAX_CONTROL_RATIO)
            control_ratio = MAX_CONTROL_RATIO;
    }
    
    if (sf_file)
    {
        ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Add soundfont: %s", sf_file);
        add_soundfont(sf_file, DEFAULT_SOUNDFONT_ORDER, -1, -1, -1);
    }
    else
    {
        ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Using internal soundfont");
    }
    
    init_load_soundfont();
    
    /* init_aq_buff */
    aq_setup();
    double time1, /* max buffer */
        time2,    /* init filled */
        base;     /* buffer of device driver */

    if (IS_STREAM_TRACE)
    {
        time1 = 0.f;
        time2 = 100.f;
        base = (double)aq_get_dev_queuesize() / play_mode->rate;
        time2 = base * time2 / 100.0;
        aq_set_soft_queue(time1, time2);
    }

    /* init resamp cache */
    if (allocate_cache_size > 0)
        resamp_cache_reset();

    /* main loop */
    //play_midi_file(files[0]);
    ctl->pass_playing_list(nfiles, files);

    if (intr)
        aq_flush(1);

    play_mode->close_output();
    ctl->close();
    /* main loop end */

    free_soft_queue();
    free_instruments(0);
    free_soundfonts();
    free_cache_data();
    free_readmidi();
    free_global_mblock();
    free_tone_bank();
    free_instrument_map();
    clean_up_pathlist();
    free_reverb_buffer();
    free_effect_buffers();
    free(voice);
    free_gauss_table();
    for (int i = 0; i < MAX_CHANNELS; i++)
        free_drum_effect(i);
    SDL_Quit();
    return 0;
}