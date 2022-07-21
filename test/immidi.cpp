#include <application.h>
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <imgui_helper.h>
#include <imgui_extra_widget.h>
#include <dir_iterate.h>
#include <ImGuiTabWindow.h>
#include <ImGuiFileDialog.h>
#ifdef __cplusplus
extern "C" {
#endif
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
#ifdef __cplusplus
}
#endif
#include <SDL.h>
#include <unistd.h>
#include <iostream>

#define APP_NAME "MIDI Player"

#if IMGUI_ICONS
#define DIGITAL_0   ICON_FAD_DIGITAL0
#define DIGITAL_1   ICON_FAD_DIGITAL1
#define DIGITAL_2   ICON_FAD_DIGITAL2
#define DIGITAL_3   ICON_FAD_DIGITAL3
#define DIGITAL_4   ICON_FAD_DIGITAL4
#define DIGITAL_5   ICON_FAD_DIGITAL5
#define DIGITAL_6   ICON_FAD_DIGITAL6
#define DIGITAL_7   ICON_FAD_DIGITAL7
#define DIGITAL_8   ICON_FAD_DIGITAL8
#define DIGITAL_9   ICON_FAD_DIGITAL9
#else
#define DIGITAL_0   "0"
#define DIGITAL_1   "1"
#define DIGITAL_2   "2"
#define DIGITAL_3   "3"
#define DIGITAL_4   "4"
#define DIGITAL_5   "5"
#define DIGITAL_6   "6"
#define DIGITAL_7   "7"
#define DIGITAL_8   "8"
#define DIGITAL_9   "9"
#endif

struct MIDI_Log
{
    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets; // Index to lines offset. We maintain this with AddLog() calls.
    bool                AutoScroll;  // Keep scrolling if already at the bottom.

    MIDI_Log()
    {
        AutoScroll = true;
        Clear();
    }

    void Clear()
    {
        Buf.clear();
        LineOffsets.clear();
        LineOffsets.push_back(0);
    }

    void AddLog(const char* fmt, ...) IM_FMTARGS(2)
    {
        int old_size = Buf.size();
        va_list args;
        va_start(args, fmt);
        Buf.appendfv(fmt, args);
        va_end(args);
        for (int new_size = Buf.size(); old_size < new_size; old_size++)
            if (Buf[old_size] == '\n')
                LineOffsets.push_back(old_size + 1);
    }

    void AddLog(const char* fmt, va_list ap)
    {
        int old_size = Buf.size();
        Buf.appendfv(fmt, ap);
        for (int new_size = Buf.size(); old_size < new_size; old_size++)
            if (Buf[old_size] == '\n')
                LineOffsets.push_back(old_size + 1);
    }

    void Draw(const char* title, ImVec2 size)
    {
        ImGui::TextUnformatted("[Log]");
        ImGui::SameLine();
        // Options menu
        if (ImGui::BeginPopup("Options"))
        {
            ImGui::Checkbox("Auto-scroll", &AutoScroll);
            ImGui::EndPopup();
        }

        // Main window
        if (ImGui::Button("Options"))
            ImGui::OpenPopup("Options");
        ImGui::SameLine();
        bool clear = ImGui::Button("Clear");
        ImGui::SameLine();
        bool copy = ImGui::Button("Copy");
        ImGui::SameLine();
        Filter.Draw("Filter", -100.0f);

        ImGui::Separator();
        ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

        if (clear)
            Clear();
        if (copy)
            ImGui::LogToClipboard();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        const char* buf = Buf.begin();
        const char* buf_end = Buf.end();
        if (Filter.IsActive())
        {
            // In this example we don't use the clipper when Filter is enabled.
            // This is because we don't have a random access on the result on our filter.
            // A real application processing logs with ten of thousands of entries may want to store the result of
            // search/filter.. especially if the filtering function is not trivial (e.g. reg-exp).
            for (int line_no = 0; line_no < LineOffsets.Size; line_no++)
            {
                const char* line_start = buf + LineOffsets[line_no];
                const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                if (Filter.PassFilter(line_start, line_end))
                    ImGui::TextUnformatted(line_start, line_end);
            }
        }
        else
        {
            // The simplest and easy way to display the entire buffer:
            //   ImGui::TextUnformatted(buf_begin, buf_end);
            // And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward
            // to skip non-visible lines. Here we instead demonstrate using the clipper to only process lines that are
            // within the visible area.
            // If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them
            // on your side is recommended. Using ImGuiListClipper requires
            // - A) random access into your data
            // - B) items all being the  same height,
            // both of which we can handle since we an array pointing to the beginning of each line of text.
            // When using the filter (in the block of code above) we don't have random access into the data to display
            // anymore, which is why we don't use the clipper. Storing or skimming through the search result would make
            // it possible (and would be recommended if you want to search through tens of thousands of entries).
            ImGuiListClipper clipper;
            clipper.Begin(LineOffsets.Size);
            while (clipper.Step())
            {
                for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
                {
                    const char* line_start = buf + LineOffsets[line_no];
                    const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                    ImGui::TextUnformatted(line_start, line_end);
                }
            }
            clipper.End();
        }
        ImGui::PopStyleVar();

        if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
    }
};

static MIDI_Log midi_log;
std::vector<std::string> sf_file_list;
std::vector<std::string> sf_name_list;
extern struct URL_module URL_module_file;
extern struct URL_module URL_module_dir;
extern struct URL_module URL_module_pipe;
struct URL_module *url_module_list[] =
{
    &URL_module_file,
    &URL_module_dir,
    &URL_module_pipe,
    NULL
};

typedef struct _MFnode
{
    char *file;
    char *title;
    struct midi_file_info *infop;
    float duration;
    float time;
    int tracks;
    int master_volume;
    int voices;
    int max_voices;
    int max_channels;
    int meas; 
    int beat;
    int tempo;
    int tempo_ratio;
    int aq_rate;
    int main_key;
    int main_key_offset;
    int time_sig_n, time_sig_d, time_sig_c, time_sig_b;
} MFnode;

typedef struct midi_event
{
    float start_time {NAN};
    float end_time {NAN};
    int vel {-1};
    int note {-1};
} MIDI_Event;

typedef struct midi_channel
{
    char instrum_name[128];
    ImGui::Piano key_board;
    int velocity;
    int volume;
    int expression;
    int program;
    int panning;
    int pitchbend;
    int sustain;
    int wheel;
    int mute;
    int bank;
    int bank_lsb;
    int bank_msb;
    int is_drum;
    int bend_mark;

    // UI Checks
    bool check_mute;
    bool check_solo;

    // UI Event
    std::vector<MIDI_Event> events;

} MIDIChannel;

struct audio_channel_data
{
    ImGui::ImMat m_wave;
    ImGui::ImMat m_fft;
    ImGui::ImMat m_db;
    ImGui::ImMat m_DBShort;
    ImGui::ImMat m_DBLong;
    ImGui::ImMat m_Spectrogram;
    ImTextureID texture_spectrogram {nullptr};
    float m_decibel {0};
    int m_DBMaxIndex {-1};
    ~audio_channel_data() { if (texture_spectrogram) ImGui::ImDestroyTexture(texture_spectrogram); }
};

static MFnode *current_MFnode = NULL;
static std::string m_midi_file;
static std::string m_wav_file;
static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static int ctl_pass_playing_list(int number_of_files, char *list_of_files[]);
static int ctl_read(int32 *valp);
static int ctl_write(char *valp, int32 size);
static int cmsg(int type, int verbosity_level, char *fmt, ...);
static void ctl_event(CtlEvent *e);
std::mutex event_mutex;
std::mutex data_mutex;
static bool m_running = false;
static bool m_converting = false;
static bool m_paused = false;
static bool show_midi_keyboard = false;
static std::vector<std::pair<int, int>> events;
static MIDIChannel midi_channels[MAX_CHANNELS];
static ImGui::Piano midi_keyboard;
static int midi_keyboard_instrum = 0;
static bool keyboard_is_drum = false;

// Scope
enum VectorScopeMode {
    LISSAJOUS,
    LISSAJOUS_XY,
    POLAR,
    MODE_NB,
};

static audio_channel_data channel_data[2];
static ImGui::ImMat AudioVector;
static ImTextureID texture_vector;
// Audio Wave Scale setting
static float AudioWaveScale    {1.0};
// Audio Vector setting
static int AudioVectorMode     {LISSAJOUS};
static float AudioVectorZoom    {1.0};
// Audio FFT Scale setting
static float AudioFFTScale    {1.0};
// Audio dB Scale setting
static float AudioDBScale    {1.0};
// Audio Spectrogram setting
static float AudioSpectrogramOffset {0.0};
static float AudioSpectrogramLight {1.0};

ControlMode imgui_control_mode =
{
    "imgui interface", 'm',
    "imgui",
    VERB_VERBOSE, 0, 0,
    CTLF_OUTPUTRAWDATA,
    ctl_open,
    ctl_close,
    ctl_pass_playing_list,
    ctl_read,
    ctl_write,
    cmsg,
    ctl_event
};

static uint32 cuepoint = 0;
static int cuepoint_pending = 0;

#define CTL_STATUS_UPDATE -98
#define CTL_STATUS_INIT -99

/* GS LCD */
#define GS_LCD_MARK_ON -1
#define GS_LCD_MARK_OFF -2
#define GS_LCD_MARK_CLEAR -3
static ImGuiFileDialog save_dialog;
static bool midi_is_selected = false;
static int sf_index = 0;
static bool b_surround = true;
static bool b_fast_decay = true;
static bool b_modulation_wheel = true;
static bool b_portamento = true;
static bool b_NRPN_vibrato = true;
static bool b_channel_pressure = true;
static bool b_trace_text_meta = true;
static bool b_overlapped_voice = true;
static bool b_temperament = true;
static bool b_keyboard_view = true;
static int resample_index = 2;
const char* resample_items[] = { "cspline", "lagrange", "gauss", "newton", "linear", "none" };
static int voice_filter_index = 1;
const char* voice_filter_items[] = { "none", "Chamberlin's lowpass filter", "Moog lowpass VCF" };
static int samplerate_index = 5;
const char* samplerate_items[] = { "8000", "11025", "16000", "22050", "24000", "44100", "48000", "96000" };
static int sample_rate = 44100;
static const char *keysig_name[] = {
	"Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F ", "C ",
	"G ", "D ", "A ", "E ", "B ", "F#", "C#", "G#",
	"D#", "A#"
};
static std::thread * play_thread = nullptr;
static std::vector<std::string> DIGITALS = {DIGITAL_0, DIGITAL_1, DIGITAL_2, DIGITAL_3, DIGITAL_4, DIGITAL_5, DIGITAL_6, DIGITAL_7, DIGITAL_8, DIGITAL_9};

// ctl function
static int ctl_open(int using_stdin, int using_stdout)
{
    imgui_control_mode.opened = 1;
    return 0;
}

static void ctl_close(void)
{
    imgui_control_mode.opened = 0;
}

static int ctl_pass_playing_list(int number_of_files, char *list_of_files[])
{
    if (number_of_files == 0 || ! list_of_files || !list_of_files[0])
        return 0;

    char *title = get_midi_title(list_of_files[0]);
    struct midi_file_info *infop = get_midi_file_info(list_of_files[0], 0);
    if (infop && infop->format >= 0)
    {
        if (!current_MFnode)
        {
            current_MFnode = (MFnode *)safe_malloc(sizeof(MFnode));
            memset(current_MFnode, 0, sizeof(MFnode));
        }
        else
        {
            memset(current_MFnode, 0, sizeof(MFnode));
        }
        current_MFnode->title = title;
        current_MFnode->file = safe_strdup(url_unexpand_home_dir(list_of_files[0]));
        current_MFnode->infop = infop;
    }
    return dumb_pass_playing_list(number_of_files, list_of_files);
}

static int ctl_read(int32 *valp)
{
    int ret = RC_NONE;
    if (cuepoint_pending)
    {
        *valp = cuepoint;
        cuepoint_pending = 0;
        return RC_FORWARD;
    }
    if (event_mutex.try_lock())
    {
        if (events.size() > 0)
        {
            auto it = events.begin();
            ret = it->first;
            *valp = it->second;
            events.erase(it);
        }
        event_mutex.unlock();
    }
    return ret;
}

static int ctl_write(char *valp, int32 size)
{
    int inBytesPerQuant;
    float maxLevel;
    if (m_converting)
        return size;
    int ch = play_mode->encoding & PE_MONO ? 1 : 2;
    if (play_mode->encoding & PE_16BIT)
    {
        inBytesPerQuant = 2;
        maxLevel = 32768.0;
    }
    else if (play_mode->encoding & PE_24BIT)
    {
        inBytesPerQuant = 3;
        maxLevel = 8388608.0;
    }
    else
        return size;
    
    int samples = size / (inBytesPerQuant * ch);
    for (int c = 0; c < ch; c++)
    {
        channel_data[c].m_wave.create_type(samples, IM_DT_FLOAT32);
    }

    switch (inBytesPerQuant)
    {
        case 2:
            for (int n = 0; n < samples; n++)
            {
                for (int c = 0; c < ch; c++)
                {
                    channel_data[c].m_wave.at<float>(n) = ((short *)valp)[n * ch + c] / maxLevel;
                }
            }
        break;
        case 3:
            for (int n = 0; n < samples; n++)
            {
                for (int c = 0; c < ch; c++)
                {
                    channel_data[c].m_wave.at<float>(n) = (*(int32 *)&valp[n * 6 + c * 3] >> 8) / maxLevel;
                }
            }
        break;
        default: break;
    }

    if (data_mutex.try_lock())
    {
        const int view_size = 256;
        for (int c = 0; c < ch; c++)
        {
            channel_data[c].m_fft.clone_from(channel_data[c].m_wave);
            ImGui::ImRFFT((float *)channel_data[c].m_fft.data, channel_data[c].m_fft.w, true);
            channel_data[c].m_db.create_type((channel_data[c].m_fft.w >> 1) + 1, IM_DT_FLOAT32);
            channel_data[c].m_DBMaxIndex = ImGui::ImReComposeDB((float*)channel_data[c].m_fft.data, (float *)channel_data[c].m_db.data, channel_data[c].m_fft.w, false);
            channel_data[c].m_DBShort.create_type(20, IM_DT_FLOAT32);
            ImGui::ImReComposeDBShort((float*)channel_data[c].m_fft.data, (float*)channel_data[c].m_DBShort.data, channel_data[c].m_fft.w);
            channel_data[c].m_DBLong.create_type(76, IM_DT_FLOAT32);
            ImGui::ImReComposeDBLong((float*)channel_data[c].m_fft.data, (float*)channel_data[c].m_DBLong.data, channel_data[c].m_fft.w);
            channel_data[c].m_decibel = ImGui::ImDoDecibel((float*)channel_data[c].m_fft.data, channel_data[c].m_fft.w);
            if (channel_data[c].m_Spectrogram.w != (channel_data[c].m_fft.w >> 1) + 1)
            {
                channel_data[c].m_Spectrogram.create_type((channel_data[c].m_fft.w >> 1) + 1, view_size, 4, IM_DT_INT8);
            }
            if (!channel_data[c].m_Spectrogram.empty())
            {
                auto w = channel_data[c].m_Spectrogram.w;
                auto _c = channel_data[c].m_Spectrogram.c;
                memmove(channel_data[c].m_Spectrogram.data, (char *)channel_data[c].m_Spectrogram.data + w * _c, channel_data[c].m_Spectrogram.total() - w * _c);
                uint32_t * last_line = (uint32_t *)channel_data[c].m_Spectrogram.row_c<uint8_t>(view_size - 1);
                for (int n = 0; n < w; n++)
                {
                    float value = channel_data[c].m_db.at<float>(n) * M_SQRT2 + 64 + AudioSpectrogramOffset;
                    value = ImClamp(value, -64.f, 63.f);
                    float light = (value + 64) / 127.f;
                    value = (int)((value + 64) + 170) % 255; 
                    auto hue = value / 255.f;
                    auto color = ImColor::HSV(hue, 1.0, light * AudioSpectrogramLight);
                    last_line[n] = color;
                }
            }
        }
        if (ch >= 2)
        {
            if (AudioVector.empty())
            {
                AudioVector.create_type(view_size, view_size, 4, IM_DT_INT8);
                AudioVector.fill((uint8_t)0);
                AudioVector.elempack = 4;
            }
            else
            {
                // fade out all pixels
                float zoom = AudioVectorZoom;
                float hw = AudioVector.w / 2;
                float hh = AudioVector.h / 2;
                int samples = channel_data[0].m_wave.w;
                AudioVector *= 0.99;
                for (int n = 0; n < samples; n++)
                {
                    float s1 = channel_data[0].m_wave.at<float>(n);
                    float s2 = channel_data[1].m_wave.at<float>(n);
                    int x = hw;
                    int y = hh;

                    if (AudioVectorMode == LISSAJOUS)
                    {
                        x = ((s2 - s1) * zoom / 2 + 1) * hw;
                        y = (1.0 - (s1 + s2) * zoom / 2) * hh;
                    }
                    else if (AudioVectorMode == LISSAJOUS_XY)
                    {
                        x = (s2 * zoom + 1) * hw;
                        y = (s1 * zoom + 1) * hh;
                    }
                    else
                    {
                        float sx, sy, cx, cy;
                        sx = s2 * zoom;
                        sy = s1 * zoom;
                        cx = sx * sqrtf(1 - 0.5 * sy * sy);
                        cy = sy * sqrtf(1 - 0.5 * sx * sx);
                        x = hw + hw * ImSign(cx + cy) * (cx - cy) * .7;
                        y = AudioVector.h - AudioVector.h * fabsf(cx + cy) * .7;
                    }
                    x = ImClamp(x, 0, AudioVector.w - 1);
                    y = ImClamp(y, 0, AudioVector.h - 1);
                    uint8_t r = ImClamp(AudioVector.at<uint8_t>(x, y, 0) + 30, 0, 255);
                    uint8_t g = ImClamp(AudioVector.at<uint8_t>(x, y, 1) + 50, 0, 255);
                    uint8_t b = ImClamp(AudioVector.at<uint8_t>(x, y, 2) + 30, 0, 255);
                    AudioVector.draw_dot(x, y, ImPixel(r / 255.0, g / 255.0, b / 255.0, 1.f));
                }
            }
        }
        data_mutex.unlock();
    }
    return size;
}

static int cmsg(int type, int verbosity_level, char *fmt, ...)
{
    va_list ap;
    if ((type == CMSG_TEXT || type == CMSG_INFO || type == CMSG_WARNING) &&
        imgui_control_mode.verbosity < verbosity_level)
        return 0;
    va_start(ap, fmt);
    std::string FMT = "[Play] - " + std::string(fmt) + "\n";
    midi_log.AddLog(FMT.c_str(), ap);
    va_end(ap);
    return 0;
}

static void reset_channel_keyboard()
{
    for (int i = 0; i < MAX_CHANNELS; i++) 
    {
        midi_channels[i].key_board.reset();
    }
}

static void reset_channel_mute()
{
    for (int i = 0; i < MAX_CHANNELS; i++) 
    {
        midi_channels[i].check_mute = false;
    }
}

static bool reset_channel_solo()
{
    bool has_solo = false;
    for (int i = 0; i < MAX_CHANNELS; i++) 
    {
        if (midi_channels[i].check_solo)
            has_solo = true;
        midi_channels[i].check_solo = false;
    }
    return has_solo;
}

static void reset_channels()
{
    for (int i = 0; i < MAX_CHANNELS; i++) 
    {
        midi_channels[i].events.clear();
        memset(&midi_channels[i], 0, sizeof(MIDIChannel));
        midi_channels[i].panning = 64;
        midi_channels[i].pitchbend = 0x2000;
    }
}

static void reset_events()
{
    events.clear();
    cuepoint = 0;
    cuepoint_pending = 0;
}

static std::vector<MIDI_Event>::iterator find_last(int channel, int note)
{
    std::vector<MIDI_Event>::iterator it = midi_channels[channel].events.end();
    while (it != midi_channels[channel].events.begin())
    {
        it --;
        if (it->note == note && isnan(it->end_time))
        {
            return it;
        }
    }

    return it;
}

static void parser_events(MidiEvent * events)
{
    if (events)
    {
        current_MFnode->max_channels = -1;
        MidiEvent *current_event = events;
        for (;;)
        {
            int type = current_event->type;
            int ch = current_event->channel;
            int32_t time = current_event->time;
            int note = current_event->a;
            int vel = current_event->b;
            if (type == ME_EOT)
            {
                break;
            }
            //if (type == 1 || type == 2)
            //    fprintf(stderr, "Midi Event %f: %d %d %d\n", (float)time / play_mode->rate, type, ch, note);
            if (ch < MAX_CHANNELS && time >= 0)
            {
                if (ch > current_MFnode->max_channels)
                {
                    current_MFnode->max_channels = ch;
                }
                if (type == ME_NOTEON)
                {
                    MIDI_Event new_event;
                    new_event.start_time = (float)time / play_mode->rate;
                    new_event.note = note;
                    new_event.vel = vel;
                    midi_channels[ch].events.push_back(new_event);
                }
                else if (type == ME_NOTEOFF)
                {
                    std::vector<MIDI_Event>::iterator it = find_last(ch, note);
                    if (it != midi_channels[ch].events.end())
                    {
                        it->end_time = (float)time / play_mode->rate;
                    }
                }
                else if (type == ME_ALL_NOTES_OFF)
                {
                    for (auto &event : midi_channels[ch].events)
                    {
                        if (event.end_time == 0)
                        {
                            event.end_time = (float)time / play_mode->rate;
                        }
                    }
                }
            }
            current_event ++;
            if (!current_event)
                break;
        }
    }
}

static std::string get_key_name()
{
    if (current_MFnode)
    {
        int k = current_MFnode->main_key;
        int ko = current_MFnode->main_key_offset;
        int i = k + ((k < 8) ? 7 : -6);
        if (ko > 0)
            for (int j = 0; j < ko; j++)
                i += (i > 10) ? -5 : 7;
        else
            for (int j = 0; j < abs(ko); j++)
                i += (i < 7) ? 5 : -7;
        return std::string(keysig_name[i]) + ((k < 8) ? " Maj " : " Min ") + "(" + (ko > 0 ? "+" : "") + std::to_string(ko) + ")";
    }
    else
        return "";
}

static std::string get_system_mode_name()
{
    std::string name = "";
    switch (play_system_mode)
    {
    case GM_SYSTEM_MODE:
        name = "[GM] ";
        break;
    case GS_SYSTEM_MODE:
        name = "[GS] ";
        break;
    case XG_SYSTEM_MODE:
        name = "[XG] ";
        break;
    case GM2_SYSTEM_MODE:
        name = "[GM2]";
        break;
    default:
        name = "     ";
        break;
    }
    return name;
}

static void ctl_note(int status, int ch, int note, int vel)
{
    midi_channels[ch].velocity = vel;
    switch (status)
    {
        case VOICE_DIE:
        midi_channels[ch].key_board.up(note);
        break;
        case VOICE_FREE:
        midi_channels[ch].key_board.up(note);
        break;
        case VOICE_ON:
        midi_channels[ch].key_board.down(note, vel);
        break;
        case VOICE_SUSTAINED:
        midi_channels[ch].key_board.down(note, -1);
        break;
        case VOICE_OFF:
        midi_channels[ch].key_board.up(note);
        break;
        case GS_LCD_MARK_ON:
        break;
        case GS_LCD_MARK_OFF:
        break;
        default: break;
    }
}

static void ctl_tempo(int t, int tr)
{
	static int lasttempo = CTL_STATUS_UPDATE;
	static int lastratio = CTL_STATUS_UPDATE;
	if (t == CTL_STATUS_UPDATE)
		t = lasttempo;
	else
		lasttempo = t;
	if (tr == CTL_STATUS_UPDATE)
		tr = lastratio;
	else
		lastratio = tr;
	t = (int) (500000 / (double) t * 120 * (double) tr / 100 + 0.5);
    current_MFnode->tempo = t;
    current_MFnode->tempo_ratio = tr;
}

static void ctl_event(CtlEvent *e)
{
    if (show_midi_keyboard)
        return;
    switch (e->type)
    {
        case CTLE_NOW_LOADING:      /* v1:filename */
            break;
        case CTLE_LOADING_DONE:     /* v1:0=success -1=error 1=terminated */
            if (e->v1 == 0)
            {
                parser_events((MidiEvent *)e->v2);
            }
            break;
        case CTLE_PLAY_START:       /* v1:nsamples */
            m_running = true;
            m_paused = false;
            if (current_MFnode)
            {
                current_MFnode->duration = ((float)e->v1) / play_mode->rate;
                current_MFnode->time = 0;
                current_MFnode->voices = 0;
                current_MFnode->aq_rate = 0;
            }
            break;
        case CTLE_PLAY_END:         /* play end */
            m_running = false;
            m_paused = false;
            break;
        case CTLE_CUEPOINT:         /* v1:nsamples */
            cuepoint = e->v1;
            cuepoint_pending = 1;
            break;
        case CTLE_CURRENT_TIME:     /* v1:secs, v2:voices */
            if (current_MFnode)
            {
                current_MFnode->time = (float)e->v1 / 1000.f;
                current_MFnode->voices = (int)e->v2;
                int devsiz;
                if ((devsiz = aq_get_dev_queuesize()) > 0)
                    current_MFnode->aq_rate = (int)(((double)(aq_filled() + aq_soft_filled()) / devsiz) * 100 + 0.5);
            }
            break;
        case CTLE_TIME_SIG:      /* v1: sig_n, v2:sig_d, v3:sig_c v4:sig_b */
            if (current_MFnode)
            {
                current_MFnode->time_sig_n = (int)e->v1;
                current_MFnode->time_sig_d = (int)e->v2;
                current_MFnode->time_sig_c = (int)e->v3;
                current_MFnode->time_sig_b = (int)e->v4;
            }
            break;
        break;
        case CTLE_TRACKS:           /* v1:tracks */
            if (current_MFnode)
            {
                current_MFnode->tracks = (int)e->v1;
            }
            break;
        case CTLE_NOTE:             /* v1:status, v2:ch, v3:note, v4:velo */
            {
                int status = (int)e->v1;
                int ch = (int)e->v2;
                int note = (int)e->v3;
                int vel = (int)e->v4;
                ctl_note(status, ch, note, vel);
            }
            break;
        case CTLE_MASTER_VOLUME:    /* v1:amp(%) */
            if (current_MFnode)
            {
                current_MFnode->master_volume = (int)e->v1;
            }
            break;
        case CTLE_METRONOME:        /* v1:measure, v2:beat */
            if (current_MFnode)
            {
                current_MFnode->meas = (float)e->v1;
                current_MFnode->beat = (int)e->v2;
            }
            break;
        case CTLE_KEYSIG:           /* v1:key sig */
            if (current_MFnode)
            {
                current_MFnode->main_key = (int)e->v1;
            }
            break;
        case CTLE_KEY_OFFSET:       /* v1:key offset */
            if (current_MFnode)
            {
                current_MFnode->main_key_offset = (int)e->v1;
            }
            break;
        case CTLE_TEMPO:            /* v1:tempo */
            if (current_MFnode)
            {
                ctl_tempo((int) e->v1, CTL_STATUS_UPDATE);
            }
            break;
        case CTLE_TIME_RATIO:       /* v1:time ratio(%) */
            if (current_MFnode)
            {
                ctl_tempo(CTL_STATUS_UPDATE, (int)e->v1);
            }
            break;
        case CTLE_TEMPER_KEYSIG:    /* v1:tuning key sig */
            break;
        case CTLE_TEMPER_TYPE:      /* v1:ch, v2:tuning type */
            break;
        case CTLE_MUTE:             /* v1:ch, v2:is_mute */
            {
                int ch = e->v1;
                midi_channels[ch].mute = e->v2;
            }
            break;
        case CTLE_PROGRAM:          /* v1:ch, v2:prog, v3:name, v4:bank,lsb.msb */
            {
                int ch = e->v1;
                char *comm = (char *)e->v3;
                unsigned int banks = e->v4;
                midi_channels[ch].program = e->v2;
                midi_channels[ch].bank = banks & 0xff;
                midi_channels[ch].bank_lsb = (banks >> 8) & 0xff;
                midi_channels[ch].bank_msb = (banks >> 16) & 0xff;
                if (midi_channels[ch].is_drum)
                {
                    //midi_channels[ch].instrum_name = ICON_FA_DRUM " Drum";
                    ImFormatString(midi_channels[ch].instrum_name, 128, "%s", ICON_FA_DRUM " Drum");
                }
                else
                {
                    //midi_channels[ch].instrum_name = comm ? std::string(comm) : "";
                    ImFormatString(midi_channels[ch].instrum_name, 128, "%s", comm ? comm : "");
                }
            }
            break;
        case CTLE_VOLUME:           /* v1:ch, v2:value */
            {
                int ch = e->v1;
                midi_channels[ch].volume = e->v2;
            }
            break;
        case CTLE_EXPRESSION:       /* v1:ch, v2:value */
            {
                int ch = e->v1;
                midi_channels[ch].expression = e->v2;
            }
            break;
        case CTLE_PANNING:          /* v1:ch, v2:value */
            {
                int ch = e->v1;
                int pan = e->v2;
                if(pan < 5) pan = 0;
                else if(pan > 123) pan = 127;
                else if(pan > 60 && pan < 68) pan = 64;
                midi_channels[ch].panning = pan;
            }
            break;
        case CTLE_SUSTAIN:          /* v1:ch, v2:value */
            {
                int ch = e->v1;
                midi_channels[ch].sustain = e->v2;
            }
            break;
        case CTLE_PITCH_BEND:       /* v1:ch, v2:value */
            {
                int ch = e->v1;
                midi_channels[ch].pitchbend = e->v2;
            }
            break;
        case CTLE_MOD_WHEEL:        /* v1:ch, v2:value */
            {
                int ch = e->v1;
                midi_channels[ch].wheel = e->v2;
            }
            break;
        case CTLE_CHORUS_EFFECT:    /* v1:ch, v2:value */
            break;
        case CTLE_REVERB_EFFECT:    /* v1:ch, v2:value */
            break;
        case CTLE_LYRIC:            /* v1:lyric-ID */
            break;
        case CTLE_REFRESH:          /* play refresh */
            break;
        case CTLE_RESET:            /* play reset */
            reset_channel_keyboard();
            break;
        case CTLE_SPEANA:           /* v1:double[] v2:len */
            break;
        case CTLE_PAUSE:            /* v1:pause on/off v2:time of pause */
            {
                int paused = (int)e->v1;
                if (paused)
                {
                    m_paused = true;
                    reset_channel_keyboard();
                }
                else
                {
                    m_paused = false;
                }
            }
            break;
        case CTLE_GSLCD:            /* GS L.C.D. */
            break;
        case CTLE_MAXVOICES:        /* v1:voices, Change voices */
            if (current_MFnode)
            {
                current_MFnode->max_voices = (int)e->v1;
            }
            break;
        case CTLE_DRUMPART:         /* v1:ch, v2:is_drum */
            {
                int ch = e->v1;
                midi_channels[ch].is_drum = e->v2;
            }
            break;
        default: break;
    }
}

static int libmidi_init(void)
{
    events.clear();
    
    ctl = &imgui_control_mode;
    play_mode = play_mode_list[3];
    
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
    play_mode->rate = sample_rate; // default

    /* save defaults */
    COPY_CHANNELMASK(drumchannels, default_drumchannels);
    COPY_CHANNELMASK(drumchannel_mask, default_drumchannel_mask);
    if (ctl->open(0, 0))
    {
        ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "Couldn't open %s (`%c')" NLS,  ctl->id_name, ctl->id_character);
        return -1;
    }
    if (play_mode->flag & PF_PCM_STREAM)
    {
        play_mode->extra_param[1] = aq_calc_fragsize();
        ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "requesting fragment size: %d", play_mode->extra_param[1]);
    }

    if (play_mode->open_output() < 0)
    {
        ctl->cmsg(CMSG_FATAL, VERB_NORMAL, "Couldn't open %s (`%c')", play_mode->id_name, play_mode->id_character);
        ctl->close();
        return -1;
    }
    if (!control_ratio)
    {
        control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
        if (control_ratio < 1)
            control_ratio = 1;
        else if (control_ratio > MAX_CONTROL_RATIO)
            control_ratio = MAX_CONTROL_RATIO;
    }
    init_load_soundfont();
    reset_channels();
    return 0;
}

static void libmidi_release()
{
    play_mode->close_output();
    ctl->close();

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
    free(voice); voice = nullptr;
    free_gauss_table();
    for (int i = 0; i < MAX_CHANNELS; i++)
        free_drum_effect(i);
    if (current_MFnode)
    {
        free(current_MFnode);
        current_MFnode = NULL;
    }
    reset_channels();
    events.clear();
}

static void play_pre()
{
    play_mode = play_mode_list[3];
    play_mode->rate = sample_rate;
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
    
    reset_midi(0);
    reset_channels();
}

static void play_post()
{
    if (intr)
        aq_flush(1);
}

static int thread_main(std::string& midi_file)
{
    int ret = 0;
    if (midi_file.empty())
        return -1;
    const char *files[1] = {midi_file.c_str()};

    play_pre();

    ret = ctl->pass_playing_list(1, (char **)files);

    play_post();

    events.clear();

    if (ret != RC_ERROR)
        midi_log.AddLog("[App ] - Finished Play\n");
    else
        midi_log.AddLog("[App ] - Play Error\n");

    return ret;
}

static int thread_convert_to_wav(std::string& midi_file, std::string& wav_file)
{
    int ret = 0;
    int32_t t1, t2;
    float speed = 0;

    if (midi_file.empty() || wav_file.empty())
        return -1;
    
    m_converting = true;
    t1 = ImGui::get_current_time_msec();
    const char *files[1] = {midi_file.c_str()};
    play_mode = play_mode_list[0];
    play_mode->rate = sample_rate;
    play_mode->name = (char *)wav_file.c_str();

    if (play_mode->open_output() < 0)
    {
        ctl->cmsg(CMSG_FATAL, VERB_NORMAL, "Couldn't open %s (`%c')", play_mode->id_name, play_mode->id_character);
        return -1;
    }

    /* init_aq_buff */
    aq_setup();

    /* init resamp cache */
    if (allocate_cache_size > 0)
        resamp_cache_reset();

    ret = ctl->pass_playing_list(1, (char **)files);

    if (ret != RC_ERROR)
    {
        t2 = ImGui::get_current_time_msec();
        speed = current_MFnode->duration * 1000 / (t2 - t1);
    }

    if (intr)
        aq_flush(1);

    play_mode->close_output();

    events.clear();
    m_converting = false;
    if (ret != RC_ERROR)
        midi_log.AddLog("[App ] - Finished Convert (%.2fX)\n", speed);
    else
        midi_log.AddLog("[App ] - Convert Error\n");

    return ret;
}

static int thread_keyboard()
{
    playmidi_stream_init(1);
    current_file_info->tracks = 1;
    play_pre();
    show_midi_keyboard = true;
    uint64_t midi_keyboard_start_time = ImGui::get_current_time_usec();
    uint64_t last_time = midi_keyboard_start_time;
    MidiEvent current_event = {0};
    int rc = RC_NONE;
    int event_count = 0;
    double rate_frac = (double)play_mode->rate / 1000000.0;
    default_program[0] = keyboard_is_drum ? 0 : midi_keyboard_instrum;
    int drum_channel = 10;
    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        if (ISDRUMCHANNEL(i)) { drum_channel = i; break; }
    }

    reset_midi(0);
    while (show_midi_keyboard)
    {
        uint64_t current_time = ImGui::get_current_time_usec();
        uint64_t time_d = current_time - last_time;
        if (time_d == 0)
        {
            usleep(10000);
            continue;
        }
        uint64_t time_offset = (current_time - midi_keyboard_start_time) * rate_frac;
        last_time = current_time;
        bool has_event = false;
        for ( int i = 0; i < 256; i++)
        {
            if (midi_keyboard.key_states[i] == -2)
            {
                midi_keyboard.up(i);
                //midi_log.AddLog("[App ] - %d up\n", i);
                current_event.channel = keyboard_is_drum ? drum_channel : 0;
                current_event.time = time_offset;
                current_event.type = ME_NOTEOFF;
                current_event.a = i;
                current_event.b = 0;
                play_event(&current_event);
                has_event = true;
                event_count++;
            }
            else if (midi_keyboard.key_states[i] > 0)
            {
                if (midi_keyboard.key_states[i] == 127)
                {
                    //midi_log.AddLog("[App ] - %d down\n", i);
                    current_event.channel = keyboard_is_drum ? drum_channel : 0;
                    midi_keyboard.key_states[i] ++;
                    current_event.time = time_offset;
                    current_event.type = ME_NOTEON;
                    current_event.a = i;
                    current_event.b = 127;
                    play_event(&current_event);
                    has_event = true;
                    event_count++;
                }
            }
        }
        if (!has_event)
        {
            current_event.channel = 0;
            current_event.time = time_offset;
            current_event.type = ME_NONE;
            current_event.a = 0;
            current_event.b = 0;
            play_event(&current_event);
        }
    }
    play_post();
    free_all_midi_file_info();
    return 0;
}

static void push_event(int type, int vol)
{
    if (event_mutex.try_lock())
    {
        events.push_back(std::pair<int, int>(type, vol));
        event_mutex.unlock();
    }
}

static void ShowMediaScopeView(int index, ImVec2 pos, ImVec2 size)
{
#define COL_SLIDER_HANDLE   IM_COL32(112, 112, 112, 255)
#define COL_GRAY_GRATICULE  IM_COL32( 96,  96,  96, 128)
#define COL_GRATICULE_DARK  IM_COL32(128,  96,   0, 128)
#define COL_GRATICULE       IM_COL32(144, 108,   0, 64)

    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetCursorScreenPos(pos);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImRect scrop_rect = ImRect(pos, pos + size);
    switch (index)
    {
        case 0:
        {
            // wave view
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##audio_wave_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    AudioWaveScale *= 0.9f;
                    if (AudioWaveScale < 0.1)
                        AudioWaveScale = 0.1;
                    ImGui::BeginTooltip();
                    ImGui::Text("Scale:%f", AudioWaveScale);
                    ImGui::EndTooltip();
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    AudioWaveScale *= 1.1f;
                    if (AudioWaveScale > 4.0f)
                        AudioWaveScale = 4.0;
                    ImGui::BeginTooltip();
                    ImGui::Text("Scale:%f", AudioWaveScale);
                    ImGui::EndTooltip();
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    AudioWaveScale = 1.f;
            }
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x, size.y / 2);
            ImGui::SetCursorScreenPos(pos);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 1.f, 0.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < 2; i++)
            {
                ImVec2 channel_min = pos + ImVec2(0, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x, channel_view_size.y * i);
                // draw graticule line
                // draw middle line
                ImVec2 p1 = ImVec2(pos.x, pos.y + channel_view_size.y * i + channel_view_size.y / 2);
                ImVec2 p2 = p1 + ImVec2(channel_view_size.x, 0);
                draw_list->AddLine(p1, p2, IM_COL32(128, 128, 128, 128));
                // draw grid
                auto grid_number = floor(10 / AudioWaveScale);
                auto grid_height = channel_view_size.y / 2 / grid_number;
                if (grid_number > 10) grid_number = 10;
                for (int x = 0; x < grid_number; x++)
                {
                    ImVec2 gp1 = p1 - ImVec2(0, grid_height * x);
                    ImVec2 gp2 = gp1 + ImVec2(channel_view_size.x, 0);
                    draw_list->AddLine(gp1, gp2, COL_GRAY_GRATICULE);
                    ImVec2 gp3 = p1 + ImVec2(0, grid_height * x);
                    ImVec2 gp4 = gp3 + ImVec2(channel_view_size.x, 0);
                    draw_list->AddLine(gp3, gp4, COL_GRAY_GRATICULE);
                }
                auto vgrid_number = channel_view_size.x / grid_height;
                for (int x = 0; x < vgrid_number; x++)
                {
                    ImVec2 gp1 = p1 + ImVec2(grid_height * x, 0);
                    ImVec2 gp2 = gp1 - ImVec2(0, grid_height * (grid_number - 1));
                    draw_list->AddLine(gp1, gp2, COL_GRAY_GRATICULE);
                    ImVec2 gp3 = gp1 + ImVec2(0, grid_height * (grid_number - 1));
                    draw_list->AddLine(gp1, gp3, COL_GRAY_GRATICULE);
                }
                if (!channel_data[i].m_wave.empty())
                {
                    ImGui::PushID(i);
                    ImGui::PlotLines("##wave", (float *)channel_data[i].m_wave.data, channel_data[i].m_wave.w, 0, nullptr, -1.0 / AudioWaveScale , 1.0 / AudioWaveScale, channel_view_size, 4, false, false);
                    ImGui::PopID();
                }
                draw_list->AddRect(channel_min, channel_max, COL_SLIDER_HANDLE, 0);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            draw_list->PopClipRect();
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, IM_COL32_WHITE, 0, 0, 1);
            ImGui::EndGroup();
        }
        break;
        case 1:
        {
            char mark[32] = {0};
            // vector view
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##audio_vector_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    AudioVectorZoom *= 0.9f;
                    if (AudioVectorZoom < 0.1)
                        AudioVectorZoom = 0.1;
                    ImGui::BeginTooltip();
                    ImGui::Text("Zoom:%f", AudioVectorZoom);
                    ImGui::EndTooltip();
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    AudioVectorZoom *= 1.1f;
                    if (AudioVectorZoom > 4.0f)
                        AudioVectorZoom = 4.0;
                    ImGui::BeginTooltip();
                    ImGui::Text("Zoom:%f", AudioVectorZoom);
                    ImGui::EndTooltip();
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    AudioVectorZoom = 1.f;
            }

            // draw graticule line
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 center_point = ImVec2(scrop_rect.Min + size / 2);
            float radius = size.x / 2;
            draw_list->AddCircle(center_point, radius, COL_GRATICULE, 0, 1);
            draw_list->AddLine(scrop_rect.Min + ImVec2(0, size.y / 2), scrop_rect.Max - ImVec2(0, size.y / 2), COL_GRATICULE);
            draw_list->AddLine(scrop_rect.Min + ImVec2(size.x / 2, 0), scrop_rect.Max - ImVec2(size.x / 2, 0), COL_GRATICULE);
            ImGui::SetWindowFontScale(0.5);
            float mark_point = size.x / 2 * AudioVectorZoom;
            float mark_val = 1.0;
            ImVec2 mark_pos = center_point;
            while (mark_point >= size.x / 2) { mark_point /= 2; mark_val /= 2; }
            mark_pos = center_point + ImVec2(- mark_point, 0);
            ImFormatString(mark, IM_ARRAYSIZE(mark), mark_val >= 0.5 ? "%-4.1f" : mark_val >= 0.25 ? "%-4.2f" : "%-4.3f", mark_val);
            draw_list->AddLine(mark_pos + ImVec2(0, -4), mark_pos + ImVec2(0, 4), COL_GRATICULE, 2);
            draw_list->AddText(mark_pos + ImVec2(-6, -12), COL_GRATICULE, mark);
            mark_pos = center_point + ImVec2(mark_point, 0);
            draw_list->AddLine(mark_pos + ImVec2(0, -4), mark_pos + ImVec2(0, 4), COL_GRATICULE, 2);
            draw_list->AddText(mark_pos + ImVec2(-6, -12), COL_GRATICULE, mark);
            mark_pos = center_point + ImVec2(0, -mark_point);
            draw_list->AddLine(mark_pos + ImVec2(-3, 0), mark_pos + ImVec2(4, 0), COL_GRATICULE, 2);
            draw_list->AddText(mark_pos + ImVec2(-6, -10), COL_GRATICULE, mark);
            mark_pos = center_point + ImVec2(0, mark_point);
            draw_list->AddLine(mark_pos + ImVec2(-3, 0), mark_pos + ImVec2(4, 0), COL_GRATICULE, 2);
            draw_list->AddText(mark_pos + ImVec2(-6, 0), COL_GRATICULE, mark);
            mark_point /= 2;
            mark_val /= 2;
            ImFormatString(mark, IM_ARRAYSIZE(mark), mark_val >= 0.5 ? "%-4.1f" : mark_val >= 0.25 ? "%-4.2f" : "%-4.3f", mark_val);
            mark_pos = center_point + ImVec2(- mark_point, 0);
            draw_list->AddLine(mark_pos + ImVec2(0, -4), mark_pos + ImVec2(0, 4), COL_GRATICULE, 2);
            draw_list->AddText(mark_pos + ImVec2(-6, -12), COL_GRATICULE, mark);
            mark_pos = center_point + ImVec2(mark_point, 0);
            draw_list->AddLine(mark_pos + ImVec2(0, -4), mark_pos + ImVec2(0, 4), COL_GRATICULE, 2);
            draw_list->AddText(mark_pos + ImVec2(-6, -12), COL_GRATICULE, mark);
            mark_pos = center_point + ImVec2(0, -mark_point);
            draw_list->AddLine(mark_pos + ImVec2(-3, 0), mark_pos + ImVec2(4, 0), COL_GRATICULE, 2);
            draw_list->AddText(mark_pos + ImVec2(-6, -10), COL_GRATICULE, mark);
            mark_pos = center_point + ImVec2(0, mark_point);
            draw_list->AddLine(mark_pos + ImVec2(-3, 0), mark_pos + ImVec2(4, 0), COL_GRATICULE, 2);
            draw_list->AddText(mark_pos + ImVec2(-6, 0), COL_GRATICULE, mark);
            ImGui::SetWindowFontScale(1.0);
            draw_list->PopClipRect();
            if (!AudioVector.empty())
            {
                ImGui::ImMatToTexture(AudioVector, texture_vector);
                draw_list->AddImage(texture_vector, scrop_rect.Min, scrop_rect.Max, ImVec2(0, 0), ImVec2(1, 1));
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, IM_COL32_WHITE, 0, 0, 1);
            ImGui::EndGroup();
        }
        break;
        case 2:
        {
            // fft view
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##audio_fft_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    AudioFFTScale *= 0.9f;
                    if (AudioFFTScale < 0.1)
                        AudioFFTScale = 0.1;
                    ImGui::BeginTooltip();
                    ImGui::Text("Scale:%f", AudioFFTScale);
                    ImGui::EndTooltip();
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    AudioFFTScale *= 1.1f;
                    if (AudioFFTScale > 4.0f)
                        AudioFFTScale = 4.0;
                    ImGui::BeginTooltip();
                    ImGui::Text("Scale:%f", AudioFFTScale);
                    ImGui::EndTooltip();
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    AudioFFTScale = 1.f;
            }
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x, size.y / 2);
            ImGui::SetCursorScreenPos(pos);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 1.f, 1.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < 2; i++)
            {
                ImVec2 channel_min = pos + ImVec2(0, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x, channel_view_size.y * i);
                // draw graticule line
                ImVec2 p1 = ImVec2(pos.x, pos.y + channel_view_size.y * i + channel_view_size.y);
                auto grid_number = floor(10 / AudioFFTScale);
                auto grid_height = channel_view_size.y / grid_number;
                if (grid_number > 20) grid_number = 20;
                for (int x = 0; x < grid_number; x++)
                {
                    ImVec2 gp1 = p1 - ImVec2(0, grid_height * x);
                    ImVec2 gp2 = gp1 + ImVec2(channel_view_size.x, 0);
                    draw_list->AddLine(gp1, gp2, COL_GRAY_GRATICULE);
                }
                auto vgrid_number = channel_view_size.x / grid_height;
                for (int x = 0; x < vgrid_number; x++)
                {
                    ImVec2 gp1 = p1 + ImVec2(grid_height * x, 0);
                    ImVec2 gp2 = gp1 - ImVec2(0, grid_height * (grid_number - 1));
                    draw_list->AddLine(gp1, gp2, COL_GRAY_GRATICULE);
                }
                if (!channel_data[i].m_fft.empty())
                {
                    ImGui::PushID(i);
                    ImGui::PlotLines("##fft", (float *)channel_data[i].m_fft.data, channel_data[i].m_fft.w, 0, nullptr, 0.0, 1.0 / AudioFFTScale, channel_view_size, 4, true, true);
                    ImGui::PopID();
                }
                draw_list->AddRect(channel_min, channel_max, COL_SLIDER_HANDLE, 0);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            draw_list->PopClipRect();
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, IM_COL32_WHITE, 0, 0, 1);
            ImGui::EndGroup();
        }
        break;
        case 3:
        {
            // db view
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##audio_db_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    AudioDBScale *= 0.9f;
                    if (AudioDBScale < 0.1)
                        AudioDBScale = 0.1;
                    ImGui::BeginTooltip();
                    ImGui::Text("Scale:%f", AudioDBScale);
                    ImGui::EndTooltip();
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    AudioDBScale *= 1.1f;
                    if (AudioDBScale > 4.0f)
                        AudioDBScale = 4.0;
                    ImGui::BeginTooltip();
                    ImGui::Text("Scale:%f", AudioDBScale);
                    ImGui::EndTooltip();
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    AudioDBScale = 1.f;
            }
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x, size.y / 2);
            ImGui::SetCursorScreenPos(pos);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 1.f, 1.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < 2; i++)
            {
                ImVec2 channel_min = pos + ImVec2(0, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x, channel_view_size.y * i);
                // draw graticule line
                ImVec2 p1 = ImVec2(pos.x, pos.y + channel_view_size.y * i + channel_view_size.y);
                auto grid_number = floor(10 / AudioDBScale);
                auto grid_height = channel_view_size.y / grid_number;
                if (grid_number > 20) grid_number = 20;
                for (int x = 0; x < grid_number; x++)
                {
                    ImVec2 gp1 = p1 - ImVec2(0, grid_height * x);
                    ImVec2 gp2 = gp1 + ImVec2(channel_view_size.x, 0);
                    draw_list->AddLine(gp1, gp2, COL_GRAY_GRATICULE);
                }
                auto vgrid_number = channel_view_size.x / grid_height;
                for (int x = 0; x < vgrid_number; x++)
                {
                    ImVec2 gp1 = p1 + ImVec2(grid_height * x, 0);
                    ImVec2 gp2 = gp1 - ImVec2(0, grid_height * (grid_number - 1));
                    draw_list->AddLine(gp1, gp2, COL_GRAY_GRATICULE);
                }
                if (!channel_data[i].m_db.empty())
                {
                    ImGui::PushID(i);
                    ImGui::ImMat db_mat_inv = channel_data[i].m_db.clone();
                    db_mat_inv += 90.f;
                    ImGui::PlotLines("##db", (float *)db_mat_inv.data, db_mat_inv.w, 0, nullptr, 0, 90.f / AudioDBScale, channel_view_size, 4, false, true);
                    ImGui::PopID();
                }
                draw_list->AddRect(channel_min, channel_max, COL_SLIDER_HANDLE, 0);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            draw_list->PopClipRect();
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, IM_COL32_WHITE, 0, 0, 1);
            ImGui::EndGroup();
        }
        break;
        case 4:
        {
            // db level short view
            ImGui::BeginGroup();
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x, size.y / 2);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.f, 1.f, 1.f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < 2; i++)
            {
                ImVec2 channel_min = pos + ImVec2(0, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x, channel_view_size.y * i);
                ImGui::PushID(i);
                ImGui::PlotHistogram("##db_level_short", (float *)channel_data[i].m_DBShort.data, channel_data[i].m_DBShort.w, 0, nullptr, 0, 100, channel_view_size, 4);
                ImGui::PopID();
                draw_list->AddRect(channel_min, channel_max, COL_SLIDER_HANDLE, 0);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            draw_list->PopClipRect();
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, IM_COL32_WHITE, 0, 0, 1);
            ImGui::EndGroup();
        }
        break;
        case 5:
        {
            // db level long view
            ImGui::BeginGroup();
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x, size.y / 2);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.f, 1.f, 1.f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < 2; i++)
            {
                ImVec2 channel_min = pos + ImVec2(0, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x, channel_view_size.y * i);
                ImGui::PushID(i);
                ImGui::PlotHistogram("##db_level_long", (float *)channel_data[i].m_DBLong.data, channel_data[i].m_DBLong.w, 0, nullptr, 0, 100, channel_view_size, 4);
                ImGui::PopID();
                draw_list->AddRect(channel_min, channel_max, COL_SLIDER_HANDLE, 0);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            draw_list->PopClipRect();
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, IM_COL32_WHITE, 0, 0, 1);
            ImGui::EndGroup();
        }
        break;
        case 6:
        {
            // spectrogram view
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##audio_spectrogram_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    AudioSpectrogramLight *= 0.9f;
                    if (AudioSpectrogramLight < 0.1)
                        AudioSpectrogramLight = 0.1;
                    ImGui::BeginTooltip();
                    ImGui::Text("Light:%f", AudioSpectrogramLight);
                    ImGui::EndTooltip();
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    AudioSpectrogramLight *= 1.1f;
                    if (AudioSpectrogramLight > 1.0f)
                        AudioSpectrogramLight = 1.0;
                    ImGui::BeginTooltip();
                    ImGui::Text("Light:%f", AudioSpectrogramLight);
                    ImGui::EndTooltip();
                }
                else if (io.MouseWheelH < -FLT_EPSILON)
                {
                    AudioSpectrogramOffset -= 5;
                    if (AudioSpectrogramOffset < -96.0)
                        AudioSpectrogramOffset = -96.0;
                    ImGui::BeginTooltip();
                    ImGui::Text("Offset:%f", AudioSpectrogramOffset);
                    ImGui::EndTooltip();
                }
                else if (io.MouseWheelH > FLT_EPSILON)
                {
                    AudioSpectrogramOffset += 5;
                    if (AudioSpectrogramOffset > 96.0)
                        AudioSpectrogramOffset = 96.0;
                    ImGui::BeginTooltip();
                    ImGui::Text("Offset:%f", AudioSpectrogramOffset);
                    ImGui::EndTooltip();
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    AudioSpectrogramLight = 1.f;
                    AudioSpectrogramOffset = 0.f;
                }
            }
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x - 80, size.y / 2);
            ImGui::SetCursorScreenPos(pos);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < 2; i++)
            {
                ImVec2 channel_min = pos + ImVec2(32, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x + 32, channel_view_size.y * i);
                ImVec2 center = channel_min + channel_view_size / 2;

                  // draw graticule line
                auto hz_step = channel_view_size.y / 11;
                ImGui::SetWindowFontScale(0.6);
                for (int i = 0; i < 11; i++)
                {
                    std::string str = std::to_string(i * 2) + "kHz";
                    ImVec2 p0 = channel_min + ImVec2(-32, channel_view_size.y - i * hz_step);
                    ImVec2 p1 = channel_min + ImVec2(-24, channel_view_size.y - i * hz_step);
                    draw_list->AddLine(p0, p1, COL_GRATICULE_DARK, 1);
                    draw_list->AddText(p1 + ImVec2(2, -9), IM_COL32_WHITE, str.c_str());
                }
                ImGui::SetWindowFontScale(1.0);
                
                if (!channel_data[i].m_Spectrogram.empty())
                {
                    ImVec2 texture_pos = center - ImVec2(channel_view_size.y / 2, channel_view_size.x / 2);
                    ImGui::ImMatToTexture(channel_data[i].m_Spectrogram, channel_data[i].texture_spectrogram);
                    ImGui::ImDrawListAddImageRotate(draw_list, channel_data[i].texture_spectrogram, texture_pos, ImVec2(channel_view_size.y, channel_view_size.x), -90.0);
                }
            }
            // draw bar mark
            for (int i = 0; i < size.y; i++)
            {
                float step = 128.0 / size.y;
                int mark_step = size.y / 9;
                float value = i * step;
                float light = value / 127.0f;
                float hue = ((int)(value + 170) % 255) / 255.f;
                auto color = ImColor::HSV(hue, 1.0, light * AudioSpectrogramLight);
                ImVec2 p0 = ImVec2(scrop_rect.Max.x - 44, scrop_rect.Max.y - i);
                ImVec2 p1 = ImVec2(scrop_rect.Max.x - 32, scrop_rect.Max.y - i);
                draw_list->AddLine(p0, p1, color);
                ImGui::SetWindowFontScale(0.6);
                if ((i % mark_step) == 0)
                {
                    int db_value = 90 - (i / mark_step) * 10;
                    std::string str = std::to_string(-db_value) + "dB";
                    if (value - 64 > 0) str = "+" + str;
                    ImVec2 p2 = ImVec2(scrop_rect.Max.x - 8, scrop_rect.Max.y - i);
                    ImVec2 p3 = ImVec2(scrop_rect.Max.x, scrop_rect.Max.y - i);
                    draw_list->AddLine(p2, p3, COL_GRATICULE_DARK, 1);
                    draw_list->AddText(p1 + ImVec2(2, db_value == 90 ? -9 : -4), IM_COL32_WHITE, str.c_str());
                }
                ImGui::SetWindowFontScale(1.0);
            }

            ImGui::PopStyleVar();
            draw_list->PopClipRect();
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, IM_COL32_WHITE, 0, 0, 1);
            ImGui::EndGroup();
        }
        break;
        default: break;
    }
}

static void draw_event(std::vector<MIDI_Event>& events, int index, ImVec2 size, float time, float duration = 5.0f)
{
    ImGui::BeginGroup();
    ImU32 color = index % 2 == 0 ? IM_COL32(40, 40, 40, 255) : IM_COL32(48, 48, 48, 255);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    draw_list->AddRectFilled(pos, pos + size, color);
    float second_length = size.x / duration;
    float note_height = size.y / 128.f;
    // check every event time
    for (auto event : events)
    {
        bool hit = false;
        if (isnan(event.start_time) || isnan(event.end_time))
            continue;
        bool bdraw_line = false;
        bool bdraw_dot = true;
        ImVec2 start_pos, end_pos;
        if (event.start_time <= time && event.end_time >= time)
        {
            // event cross start time pos
            start_pos.x = pos.x;
            end_pos.x = pos.x + (event.end_time - time) * second_length;
            end_pos.y = start_pos.y = pos.y + size.y - event.note * note_height;
            hit = true;
            bdraw_line = true;
        }
        else if (event.start_time > time && event.end_time < time + duration)
        {
            // event inside event bar
            start_pos.x = pos.x + (event.start_time - time) * second_length;
            end_pos.x = pos.x + (event.end_time - time) * second_length;
            end_pos.y = start_pos.y = pos.y + size.y - event.note * note_height;
            bdraw_line = true;
        }
        else if (event.start_time > time && event.start_time < time + duration && event.end_time > time + duration)
        {
            // event crose end time pos
            start_pos.x = pos.x + (event.start_time - time) * second_length;
            end_pos.x = pos.x + duration * second_length;
            end_pos.y = start_pos.y = pos.y + size.y - event.note * note_height;
            bdraw_line = true;
        }
        if (event.end_time > time + duration)
            bdraw_dot = false;
        if (bdraw_line)
        {
            if (hit)
                draw_list->AddCircleFilled(start_pos, 3, IM_COL32(255, 64, 255, 255));
            else
                draw_list->AddCircle(start_pos, 2, IM_COL32(0, 255, 0, 224));
            draw_list->AddLine(start_pos, end_pos, IM_COL32(255, 255, 255, event.vel * 2));
            if (bdraw_dot) draw_list->AddCircle(end_pos, 2, IM_COL32(255, 0, 0, 224));
        }
    }
    ImGui::InvisibleButton("##event", size);
    ImGui::EndGroup();
}

static std::string ReplaceDigital(const std::string str)
{
    if (str.empty())
        return str;
    std::string result = "";
    for (auto c : str)
    {
        if (c >= 0x30 && c <= 0x39)
            result += DIGITALS[c - 0x30];
        else
            result += c;
    }
    return result;
}

void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = APP_NAME;
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    property.power_save = true;
    property.width = 1680;
    property.height = 960;
}

void Application_SetupContext(ImGuiContext* ctx)
{
#ifdef USE_BOOKMARK
    ImGuiSettingsHandler bookmark_ini_handler;
    bookmark_ini_handler.TypeName = "BookMark";
    bookmark_ini_handler.TypeHash = ImHashStr("BookMark");
    bookmark_ini_handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void*
    {
        return ImGuiFileDialog::Instance();
    };
    bookmark_ini_handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) -> void
    {
        IGFD::FileDialog * dialog = (IGFD::FileDialog *)entry;
        dialog->DeserializeBookmarks(line);
    };
    bookmark_ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
    {
        ImGuiContext& g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        auto bookmark = ImGuiFileDialog::Instance()->SerializeBookmarks();
        out_buf->appendf("[%s][##%s]\n", handler->TypeName, handler->TypeName);
        out_buf->appendf("%s\n", bookmark.c_str());
        out_buf->append("\n");
    };
    ctx->SettingsHandlers.push_back(bookmark_ini_handler);
#endif
}

void Application_Initialize(void** handle)
{
#if !IMGUI_APPLICATION_PLATFORM_SDL2
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);
#endif
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    auto application_setting_path = ImGuiHelper::settings_path(APP_NAME);
    auto exec_path = ImGuiHelper::exec_path();
    // init sound font list
    sf_file_list.push_back("internal");
    sf_name_list.push_back("internal");
    auto soundfont_path = application_setting_path + "soundfont";
    if (!ImGuiHelper::file_exists(soundfont_path))
    {
        if (ImGuiHelper::file_exists(application_setting_path))
        {
            ImGuiHelper::create_directory(soundfont_path);
        }
    }
    
    // add internal soundfonts
    auto installed_font_path = 
#if defined(__APPLE__)
        exec_path + "../Resources/soundfont";
#elif defined(_WIN32)
        exec_path + "../soundfont";
#elif defined(__linux__)
        exec_path + "../soundfont";
#else
        // TODO::Dicky
        std::string();
#endif
    if (ImGuiHelper::file_exists(installed_font_path))
    {
        auto gm_path = installed_font_path + PATH_SEP + "midi-gm.cfg";
        if (ImGuiHelper::file_exists(gm_path))
        {
            sf_file_list.push_back(gm_path);
            sf_name_list.push_back("midi-gm");
        }
        auto mini_path = installed_font_path + PATH_SEP + "midi-mini.cfg";
        if (ImGuiHelper::file_exists(mini_path))
        {
            sf_file_list.push_back(mini_path);
            sf_name_list.push_back("midi-mini");
        }
    }

    std::vector<std::string> file_list, name_list;
    std::vector<std::string> suffix_filter = {"cfg", "sf2"};
    DIR_Iterate(soundfont_path, file_list, name_list, suffix_filter, false, false);
    for (auto f : file_list) sf_file_list.push_back(f);
    for (auto n : name_list) sf_name_list.push_back(n);
    
    auto demo_path = 
#if defined(__APPLE__)
        exec_path + "../Resources/midi_demo";
#elif defined(_WIN32)
        exec_path + "../midi_demo";
#elif defined(__linux__)
        exec_path + "../midi_demo";
#else
        // TODO::Dicky
        application_setting_path + "midi_demo";
#endif

    ImGuiFileDialog::Instance()->OpenDialog("embedded", ICON_IGFD_FOLDER_OPEN " Choose File", 
                                                            "MIDI files (*.mid *.midi){.mid,.midi}",
                                                            m_midi_file.empty() ? demo_path + PATH_SEP : m_midi_file,
                                                            -1, 
                                                            nullptr, 
                                                            ImGuiFileDialogFlags_NoDialog | 
                                                            ImGuiFileDialogFlags_NoButton |
                                                            ImGuiFileDialogFlags_PathDecompositionShort |
                                                            ImGuiFileDialogFlags_DisableBookmarkMode | 
                                                            ImGuiFileDialogFlags_DisableCreateDirectoryButton | 
                                                            ImGuiFileDialogFlags_ReadOnlyFileNameField |
                                                            ImGuiFileDialogFlags_CaseInsensitiveExtention);
    libmidi_init();
}

void Application_Finalize(void** handle)
{
    libmidi_release();
#if !IMGUI_APPLICATION_PLATFORM_SDL2
    SDL_Quit();
#endif
}

bool Application_Frame(void * handle, bool app_will_quit)
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking;
    if (multiviewport)
    {
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    }
    else
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_None);
    }

    struct Funcs { static bool ItemGetter(void* data, int n, const char** out_str)
    {
        std::vector<std::string>* list = (std::vector<std::string>*)data;
        if (n >= list->size())
            return false;
        *out_str = list->at(n).c_str();
        return true; 
    }};
    //ImGui::Text("Exec Path:%s", ImGuiHelper::exec_path().c_str()); // for Test
    ImGui::Begin("Content", nullptr, flags);

    if (show_midi_keyboard)
    {
        ImGui::OpenPopup("MIDI Keyboard", ImGuiPopupFlags_AnyPopup);
    }
    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("MIDI Keyboard", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        midi_keyboard.draw_keyboard(ImVec2(1440, 160), true);
        ImGui::TextUnformatted(ICON_FA_GUITAR " Instrument:"); ImGui::SameLine();
        ImGui::PushItemWidth(400);
        if (ImGui::Combo("##Instrument", &midi_keyboard_instrum, default_instrum_name_list, 128))
        {
            default_program[0] = midi_keyboard_instrum;
            reset_midi(0);
        }
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 40);
        ImGui::TextUnformatted(ICON_FA_DRUM " Drum"); ImGui::SameLine();
        ImGui::Checkbox("##Drum", &keyboard_is_drum);
        ImGui::SameLine(1400);
        if (ImGui::Button("Close")) 
        { 
            show_midi_keyboard = false;
            if (play_thread && play_thread->joinable())
            {
                play_thread->join();
                delete play_thread;
                play_thread = nullptr;
            }
            default_program[0] = DEFAULT_PROGRAM;
            reset_midi(0);
            midi_keyboard.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }

    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    ImGui::PushID("##Control_Panel_Main");
    float main_width = 0.25 * window_size.x;
    float event_width = 0.75 * window_size.x;
    ImGui::Splitter(true, 4.0f, &main_width, &event_width, 96, 96);
    ImGui::PopID();
    // add control window
    ImVec2 control_pos = window_pos + ImVec2(4, 0);
    ImVec2 control_size(main_width - 4, window_size.y - 4);
    ImGui::SetNextWindowPos(control_pos, ImGuiCond_Always);
    if (ImGui::BeginChild("##Control_Panel_Window", control_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::BeginDisabled(m_running);
        // File Dialog
        if (ImGuiFileDialog::Instance()->Display("embedded", ImGuiWindowFlags_NoCollapse, ImVec2(0,0), ImVec2(0,400)))
        {
            if (ImGuiFileDialog::Instance()->IsOk())
            {
                m_midi_file = ImGuiFileDialog::Instance()->GetFilePathName();
                auto file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
                midi_is_selected = true;
                midi_log.AddLog("[App ] - Select midi file: %s\n", file_name.c_str());
                play_thread = new std::thread(thread_main, std::ref(m_midi_file));
                midi_log.AddLog("[App ] - Start Play\n");
            }
        }
        if (save_dialog.Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, ImVec2(400,600), ImVec2(FLT_MAX, FLT_MAX)))
        {
            if (!m_running && save_dialog.IsOk())
            {
                m_wav_file = save_dialog.GetFilePathName();
                play_thread = new std::thread(thread_convert_to_wav, std::ref(m_midi_file), std::ref(m_wav_file));
                midi_log.AddLog("[App ] - Start Convert\n");
            }
            save_dialog.Close();
        }
        ImGui::EndDisabled();
        ImGui::Separator();
        // Setting
        // Select SoundFont
        ImGui::BeginDisabled(m_running);
        int _index = sf_index;
        if (ImGui::Combo("Select Sound Font", &_index, &Funcs::ItemGetter, &sf_name_list, sf_name_list.size()))
        {
            if (sf_index != 0)
            {
                // remove last soundfont
                remove_soundfont((char *)sf_file_list[sf_index].c_str());
                free_instruments(0);
                free_soundfonts();
            }
            sf_index = _index;
            if (sf_index != 0)
            {
                std::string font_file = sf_file_list[sf_index];
                bool font_is_cfg = false;
                auto pos = font_file.find_last_of(".");
                if (pos != std::string::npos)
                {
                    std::string suffix = font_file.substr(pos + 1);
                    if (suffix.compare("cfg") == 0)
                        font_is_cfg = true;
                }
                if (font_is_cfg)
                    load_font_from_cfg((char *)font_file.c_str());
                else
                    add_soundfont((char *)font_file.c_str(), DEFAULT_SOUNDFONT_ORDER, -1, -1, -1);
            }
            init_load_soundfont();
        }
        if (ImGui::Combo("Resample algorithm", &resample_index, resample_items, IM_ARRAYSIZE(resample_items)))
        {
            switch (resample_index)
            {
                case 0 : set_current_resampler(RESAMPLE_CSPLINE); break;
                case 1 : set_current_resampler(RESAMPLE_LAGRANGE); break;
                case 2 : set_current_resampler(RESAMPLE_GAUSS); break;
                case 3 : set_current_resampler(RESAMPLE_NEWTON); break;
                case 4 : set_current_resampler(RESAMPLE_LINEAR); break;
                case 5 : set_current_resampler(RESAMPLE_NONE); break;
                default: break;
            }
        }
        if (ImGui::Combo("Filter algorithm", &voice_filter_index, voice_filter_items, IM_ARRAYSIZE(voice_filter_items)))
        {
            opt_lpf_def = voice_filter_index;
        }

        if (ImGui::Combo("Sample Rate", &samplerate_index, samplerate_items, IM_ARRAYSIZE(samplerate_items)))
        {
            sample_rate = atoi(samplerate_items[samplerate_index]);
            if (play_mode->fd != -1)
            {
                play_mode->close_output();
            }
            if (sample_rate < 16000)
                antialiasing_allowed = 1;
            else
                antialiasing_allowed = 0;
            play_mode->rate = sample_rate;
            play_mode->open_output();
        }

        ImGui::EndDisabled();
        // Toggle Setting
        ImGui::Checkbox("Piano Keyboard", &b_keyboard_view);
        if (ImGui::Checkbox("Surround Sound", &b_surround)) opt_surround_chorus = b_surround ? 1 : 0;
        if (ImGui::Checkbox("Fast Decay", &b_fast_decay)) fast_decay = b_fast_decay ? 1 : 0;
        if (ImGui::Checkbox("Modulation Wheel", &b_modulation_wheel)) opt_modulation_wheel = b_modulation_wheel ? 1 : 0;
        if (ImGui::Checkbox("Portamento", &b_portamento)) opt_portamento = b_portamento ? 1 : 0;
        if (ImGui::Checkbox("NRPN vibrato", &b_NRPN_vibrato)) opt_nrpn_vibrato = b_NRPN_vibrato ? 1 : 0;
        if (ImGui::Checkbox("Channel Pressure", &b_channel_pressure)) opt_channel_pressure = b_channel_pressure ? 1 : 0;
        if (ImGui::Checkbox("Trace Text Meta", &b_trace_text_meta)) opt_trace_text_meta_event = b_trace_text_meta ? 1 : 0;
        if (ImGui::Checkbox("Overlapped Voice", &b_overlapped_voice)) opt_overlap_voice_allow = b_overlapped_voice ? 1 : 0;
        if (ImGui::Checkbox("Temperament", &b_temperament)) opt_temper_control = b_temperament ? 1 : 0;

        ImGui::Separator();

        // Log Window
        auto log_pos = ImGui::GetCursorScreenPos();
        ImVec2 log_size = ImVec2(control_size.x - 8, control_size.y - (log_pos.y - control_pos.y));
        midi_log.Draw("Log##Midi Log", log_size);
    }
    ImGui::EndChild();

    ImVec2 main_sub_pos = window_pos + ImVec2(main_width + 16, 0);
    ImVec2 main_sub_size(event_width - 32, window_size.y - 4);
    ImGui::SetNextWindowPos(main_sub_pos, ImGuiCond_Always);
    if (ImGui::BeginChild("##Event_Window", main_sub_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
    {
        ImVec2 event_window_pos = ImGui::GetWindowPos();
        ImVec2 event_window_size = ImGui::GetWindowSize();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::Text("MIDI File: %s", (current_MFnode && current_MFnode->file) ? current_MFnode->file : "");
        ImGui::Text("MIDI Title: %s %s", (current_MFnode && current_MFnode->title) ? current_MFnode->title : "", get_system_mode_name().c_str());
        ImGui::Dummy(ImVec2(0, 16));
        ImGui::Text("Master Volume: %d%%", current_MFnode ? current_MFnode->master_volume : 0);
        ImGui::Text("Tempo: %d(%d%%)", current_MFnode ? current_MFnode->tempo : 0, current_MFnode ? current_MFnode->tempo_ratio : 0);
        ImGui::Text("Key: %s", get_key_name().c_str());
        // up/down key
        ImGui::SetWindowFontScale(0.75);
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(ImVec2(event_window_pos.x + 120, ImGui::GetCursorScreenPos().y));
        if (ImGui::Button(ICON_FA_CIRCLE_MINUS "##key_down", ImVec2(16, 16))) { if (m_running) push_event(RC_KEYDOWN, -1); }
        ImGui::ShowTooltipOnHover("Key Down");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_CIRCLE_PLUS "##key_up", ImVec2(16, 16))) { if (m_running) push_event(RC_KEYUP, 1); }
        ImGui::ShowTooltipOnHover("Key Up");
        ImGui::SetWindowFontScale(1.0);

        ImGui::Text("Voices: %03d / %d", current_MFnode ? current_MFnode->voices : 0, current_MFnode && current_MFnode->max_voices ? current_MFnode->max_voices : max_voices);
        ImGui::Text("Audio queue: %d%%", current_MFnode ? current_MFnode->aq_rate : 0);
        ImGui::PopStyleColor(3);
        ImGui::Dummy(ImVec2(0, 16));

        ImGui::PushItemWidth(event_window_size.x);
        float pos = current_MFnode ? current_MFnode->time : 0;
        float duration = current_MFnode ? current_MFnode->duration : 0;
        if (ImGui::SliderFloat("##timeline", &pos, 0.f, duration, "%.1f"))
        {
            int new_pos = pos * play_mode->rate;
            push_event(RC_JUMP, new_pos);
        }
        ImGui::Separator();
        ImGui::PopItemWidth();

        ImVec2 table_pos = ImGui::GetCursorScreenPos();

        // Play Control view
        ImVec2 control_view_pos = event_window_pos + ImVec2(200, 40);
        ImVec2 control_view_size = ImVec2(event_window_size.x - 200, table_pos.y - 24 - 56);
        ImGui::SetNextWindowPos(control_view_pos);
        if (ImGui::BeginChild("##Control_Window", control_view_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
        {
            draw_list->AddRectFilled(control_view_pos, control_view_pos + control_view_size, IM_COL32(32, 32, 32, 255), 4.0, ImDrawFlags_RoundCornersAll);
            const int time_area_x = 400;
            // time code
            ImVec2 time_view_pos = control_view_pos + ImVec2(20, 20);
            ImVec2 time_view_size = ImVec2(time_area_x - 40, control_view_size.y - 40);
            draw_list->AddRectFilled(time_view_pos, time_view_pos + time_view_size, IM_COL32(16, 16, 16, 255), 4.0, ImDrawFlags_RoundCornersAll);
            int secs = 0, mins = 0, tsecs = 0, tmins = 0;
            if (current_MFnode)
            {
                mins = current_MFnode->time / 60;
                secs = current_MFnode->time - mins * 60;
                tmins = current_MFnode->duration / 60;
                tsecs = current_MFnode->duration - tmins * 60;
            }
            ImGui::SetWindowFontScale(2.0);
            std::string digital_str;
            char time_str[64] = {0};
            ImFormatString(time_str, 64, " %02d:%02d / %02d:%02d", mins, secs, tmins, tsecs);
            digital_str = ReplaceDigital(std::string(time_str));
            draw_list->AddText(time_view_pos, IM_COL32(0, 255, 0, 255), digital_str.c_str());

            ImFormatString(time_str, 64, "       %04d - %d/%d", current_MFnode ? current_MFnode->meas : 0, 
                                                                current_MFnode ? current_MFnode->beat : 0,
                                                                //current_MFnode ? current_MFnode->time_sig_n : 0,
                                                                current_MFnode ? (current_MFnode->time_sig_d > 0 ? current_MFnode->time_sig_d : 4) : 0);
            digital_str = ReplaceDigital(std::string(time_str));
            draw_list->AddText(time_view_pos + ImVec2(0, 40), IM_COL32(0, 255, 0, 255), digital_str.c_str());
            ImGui::SetWindowFontScale(1.0);

            // Play control
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.5));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));
            ImGui::BeginDisabled(!midi_is_selected);
            ImGui::SetWindowFontScale(2.0);
            ImGui::SetCursorScreenPos(control_view_pos + ImVec2(20 + time_area_x, (control_view_size.y - 64) / 2));
            if (ImGui::Button(m_running ? (m_paused ? ICON_FA_PLAY : ICON_FA_PAUSE) : ICON_FA_PLAY "##play_pause", ImVec2(64, 64)))
            {
                if (!m_running)
                {
                    play_thread = new std::thread(thread_main, std::ref(m_midi_file));
                    midi_log.AddLog("[App ] - Start Play\n");
                }
                else
                {
                    push_event(RC_TOGGLE_PAUSE, 0);
                    if (m_paused)
                        midi_log.AddLog("[App ] - Resume Play\n");
                    else
                        midi_log.AddLog("[App ] - Pause Play\n");
                }
            }
            ImGui::ShowTooltipOnHover("%s", m_running ? (m_paused ? "Play" : "Pause") : "Play");

            ImGui::SetCursorScreenPos(control_view_pos + ImVec2(20 + time_area_x + 64, (control_view_size.y - 64) / 2));
            if (ImGui::Button(ICON_FA_STOP "##stop", ImVec2(64, 64)))
            {
                if (m_running) push_event(RC_STOP, 0);
            }
            ImGui::ShowTooltipOnHover("%s", "Stop");
            ImGui::EndDisabled();

            ImGui::BeginDisabled(m_running);
            ImGui::SetCursorScreenPos(control_view_pos + ImVec2(20 + time_area_x + 64 + 64, (control_view_size.y - 64) / 2));
            if (ImGui::Button(ICON_FA_FLOPPY_DISK "##save_to wav", ImVec2(64, 64)))
            {
                save_dialog.OpenDialog("ChooseFileDlgKey", ICON_IGFD_SAVE " Choose a File", 
                                        "Wav files (*.wav){.wav}",
                                        ".", "", 1, 
                                        IGFDUserDatas("SaveFile"), 
                                        ImGuiFileDialogFlags_ConfirmOverwrite |
                                        ImGuiFileDialogFlags_CaseInsensitiveExtention |
                                        ImGuiFileDialogFlags_Modal);
            }
            ImGui::ShowTooltipOnHover("%s", "Save to Wav");
            
            ImGui::SetCursorScreenPos(control_view_pos + ImVec2(20 + time_area_x + 64 + 64 + 64, (control_view_size.y - 64) / 2));
            if (ImGui::Button(ICON_FA_RULER_HORIZONTAL "##keyboard", ImVec2(64, 64)))
            {
                play_thread = new std::thread(thread_keyboard);
            }
            ImGui::ShowTooltipOnHover("%s", "Show Keyboard");
            ImGui::EndDisabled();
            ImGui::SetWindowFontScale(1.0);
            ImGui::PopStyleColor(3);

            // Play Setting
            ImGui::SetCursorScreenPos(control_view_pos + ImVec2(30 + time_area_x + 4 * 64 + 50, 0));
            ImVec4 color_offset = ImVec4(0.2f, 0.2f, 0.2f, 0.f);
            ImVec4 base_color = ImVec4(0.2f, 0.2f, 0.2f, 1.f);
            ImVec4 active_color = ImVec4(0.4f, 0.4f, 0.4f, 1.f);
            ImVec4 hovered_color = ImVec4(0.5f, 0.5f, 0.5f, 1.f);
            ImVec4 white_color = ImVec4(1.f, 1.f, 1.f, 1.f);
            ImGui::ColorSet circle_color = {base_color, active_color, hovered_color};
            ImGui::ColorSet wiper_color = {base_color + color_offset, active_color + color_offset, hovered_color + color_offset};
            ImGui::ColorSet track_color = {base_color - color_offset, active_color - color_offset, hovered_color - color_offset};
            ImGui::ColorSet tick_color = {white_color, white_color, white_color};
            float knob_size = 84.f;
            float knob_step = NAN;
            ImGui::BeginDisabled(!m_running);
            float main_volume = current_MFnode ? current_MFnode->master_volume : 0;
            float drum_power = opt_drum_power;
            float speed = current_MFnode ? current_MFnode->tempo_ratio : 0;
            if (ImGui::Knob("Main Power", &main_volume, 0.0f, 400.0f, 1.0f, 100.f, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_TICK_WIPER, "%.0f%%"))
            {
                if (current_MFnode) current_MFnode->master_volume = main_volume;
                push_event(RC_SET_VOLUME, current_MFnode->master_volume);
            }
            ImGui::SameLine();
            if (ImGui::Knob("Drum Power", &drum_power, 0.0f, 200.0f, 1.0f, 100.f, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_TICK_WIPER, "%.0f%%"))
            {
                opt_drum_power = drum_power;
            }
            ImGui::SameLine();
            if (ImGui::Knob("Speed", &speed, NAN, NAN, 0.01f, 100, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_TICK_WIPER, "%.0f%%"))
            {
                float old_speed = current_MFnode ? current_MFnode->tempo_ratio : 0;
                if (fabs(speed - old_speed) > 0.1)
                {
                    if (speed > old_speed)
                        push_event(RC_SPEEDUP, 1);
                    else
                        push_event(RC_SPEEDDOWN, 1);
                }
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();

        // show midi channels
        ImGui::SetCursorScreenPos(table_pos);
        const int max_channels = current_MFnode ? current_MFnode->max_channels + 1 : 16;
        const float channel_hight = 32;
        static ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;
        ImVec2 keyboard_size(ImVec2(ImGui::GetWindowWidth() - 96 - 16 - 20 - 20 - 32 - 32 - 6 * 50 - 11 * 8, channel_hight - 2));
        ImVec2 table_size = ImVec2(ImGui::GetWindowWidth() - 80, 16 * (channel_hight + 2) + 20);
        if (ImGui::BeginTable("midi_channels", 11, flags, table_size))
        {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("Ch", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed, 16); // Make the first column not hideable to match our use of TableSetupScrollFreeze()
            ImGui::TableSetupColumn("M##ute", ImGuiTableColumnFlags_WidthFixed, 20);
            ImGui::TableSetupColumn("S##olo", ImGuiTableColumnFlags_WidthFixed, 20);
            ImGui::TableSetupColumn("Prog", ImGuiTableColumnFlags_WidthFixed, 32);
            ImGui::TableSetupColumn("Vel", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Vol", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Expr", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Pan", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Pit", ImGuiTableColumnFlags_WidthFixed, 20);
            ImGui::TableSetupColumn("Instrument", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Keyboard", ImGuiTableColumnFlags_WidthFixed, keyboard_size.x);
            ImGui::TableHeadersRow();
            int index = 0;
            for (int i = 0; i < max_channels; i++)
            {
                if (midi_channels[i].events.empty())
                    continue;
                ImGui::PushID(i);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, channel_hight);
                for (int column = 0; column < 11; column++)
                {
                    if (!ImGui::TableSetColumnIndex(column) && column > 0)
                        continue;
                    switch (column)
                    {
                        case 0: // Ch
                            ImGui::Text("%02d", i + 1);
                        break;
                        case 1: // Mute
                            ImGui::SetWindowFontScale(0.8);
                            if (ImGui::Checkbox("##Channel Mute", &midi_channels[i].check_mute))
                            {
                                bool need_clear = reset_channel_solo();
                                if (m_running)
                                {
                                    if (need_clear) push_event(RC_MUTE_CLEAR, i);
                                    push_event(RC_TOGGLE_MUTE, i);
                                }
                            }
                            ImGui::SetWindowFontScale(1.0);
                        break;
                        case 2: // Solo
                            ImGui::SetWindowFontScale(0.8);
                            if (ImGui::Checkbox("##Channel Solo", &midi_channels[i].check_solo))
                            {
                                bool status = midi_channels[i].check_solo;
                                reset_channel_solo();
                                reset_channel_mute();
                                midi_channels[i].check_solo = status;
                                int ctl_cmd = midi_channels[i].check_solo ? RC_SOLO_PLAY : RC_MUTE_CLEAR;
                                if (m_running) push_event(ctl_cmd, i);
                            }
                            ImGui::SetWindowFontScale(1.0);
                        break;
                        case 3: // Program
                            ImGui::Text("%02d", midi_channels[i].program);
                        break;
                        case 4: // Velocity
                        {
                            float val = midi_channels[i].velocity / 128.f;
                            ImGui::ProgressBar(val, ImVec2(48, channel_hight - 2), "");
                        }
                        break;
                        case 5: // Volume
                        {
                            float val = midi_channels[i].volume / 128.f;
                            ImGui::ProgressBar(val, ImVec2(48, channel_hight - 2), "");
                        }
                        break;
                        case 6: // Expression
                        {
                            float val = midi_channels[i].expression / 128.f;
                            ImGui::ProgressBar(val, ImVec2(48, channel_hight - 2), "");
                        }
                        break;
                        case 7: // Panning
                        {
                            float val = (midi_channels[i].panning / 128.f) - 0.5f;
                            ImGui::ProgressBarPanning(val, ImVec2(48, channel_hight - 2));
                        }
                        break;
                        case 8: // Pitch Bend
                        {
                            int val = midi_channels[i].pitchbend;
                            if (midi_channels[i].wheel)
                                ImGui::TextUnformatted(" = ");
                            else if (val > 0x2000)
                                ImGui::TextUnformatted(" |>");
                            else if (val < 0x2000)
                                ImGui::TextUnformatted("<| ");
                            else
                                ImGui::TextUnformatted(" | ");
                        }
                        break;
                        case 9: // Instrum Name
                            if (midi_channels[i].is_drum)
                                ImGui::Text("%s", ICON_FA_DRUM " Drum");
                            else
                                ImGui::Text("%s", midi_channels[i].instrum_name);
                        break;
                        case 10:
                            if (b_keyboard_view) midi_channels[i].key_board.draw_keyboard(keyboard_size);
                            else draw_event(midi_channels[i].events, index, keyboard_size, current_MFnode ? current_MFnode->time : 0);
                        break;
                        default: break;
                    }
                }
                ImGui::PopID();
                index++;
            }
            ImGui::EndTable();
        }

        // draw audio meters
        ImVec2 AudioMeterPos;
        ImVec2 AudioMeterSize;
        AudioMeterPos = table_pos + ImVec2(event_window_size.x - 80, 16);
        AudioMeterSize = ImVec2(32, 16 * (channel_hight + 2));
        ImVec2 AudioUVLeftPos = AudioMeterPos + ImVec2(36, 0);
        ImVec2 AudioUVLeftSize = ImVec2(12, AudioMeterSize.y);
        ImVec2 AudioUVRightPos = AudioMeterPos + ImVec2(36 + 16, 0);
        ImVec2 AudioUVRightSize = AudioUVLeftSize;
        
        for (int i = 0; i <= 96; i+= 5)
        {
            float mark_step = AudioMeterSize.y / 96.0f;
            ImVec2 MarkPos = AudioMeterPos + ImVec2(0, i * mark_step);
            if (i % 10 == 0)
            {
                std::string mark_str = i == 0 ? "  0" : "-" + std::to_string(i);
                draw_list->AddLine(MarkPos + ImVec2(20, 8), MarkPos + ImVec2(30, 8), IM_COL32(128, 128, 128, 255), 1);
                ImGui::SetWindowFontScale(0.75);
                draw_list->AddText(MarkPos + ImVec2(0, 2), IM_COL32(128, 128, 128, 255), mark_str.c_str());
                ImGui::SetWindowFontScale(1.0);
            }
            else
            {
                draw_list->AddLine(MarkPos + ImVec2(25, 8), MarkPos + ImVec2(30, 8), IM_COL32(128, 128, 128, 255), 1);
            }
        }

        static int left_stack = 0;
        static int left_count = 0;
        static int right_stack = 0;
        static int right_count = 0;
        int l_level = channel_data[0].m_decibel;
        int r_level = channel_data[1].m_decibel;
        ImGui::SetCursorScreenPos(AudioUVLeftPos);
        ImGui::UvMeter("##luv", AudioUVLeftSize, &l_level, 0, 100, AudioUVLeftSize.y / 4, &left_stack, &left_count);
        ImGui::SetCursorScreenPos(AudioUVRightPos);
        ImGui::UvMeter("##ruv", AudioUVRightSize, &r_level, 0, 100, AudioUVRightSize.y / 4, &right_stack, &right_count);

        ImGui::Separator();
        
        auto scope_pos = ImGui::GetCursorScreenPos();
        ImVec2 view_size = ImVec2(event_window_size.x / 7, event_window_size.x / 7);
        
        data_mutex.lock();
        ShowMediaScopeView(0, scope_pos + ImVec2(view_size.x * 0, 0), view_size);
        ShowMediaScopeView(1, scope_pos + ImVec2(view_size.x * 1, 0), view_size);
        ShowMediaScopeView(3, scope_pos + ImVec2(view_size.x * 2, 0), view_size);
        ShowMediaScopeView(5, scope_pos + ImVec2(view_size.x * 3, 0), view_size + ImVec2(view_size.x, 0));
        ShowMediaScopeView(6, scope_pos + ImVec2(view_size.x * 5, 0), view_size + ImVec2(view_size.x, 0));
        data_mutex.unlock();

    }
    ImGui::EndChild();

    ImGui::End();
    if (app_will_quit && m_running)
    {
        push_event(RC_STOP, 0);
        if (play_thread && play_thread->joinable())
        {
            play_thread->join();
            delete play_thread;
            play_thread = nullptr;
        }
    }
    return app_will_quit;
}
