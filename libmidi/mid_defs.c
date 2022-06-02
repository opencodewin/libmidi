#include "common.h"
/* #define MID_OFFICIAL_NAME */
char *mid2name(int mid)
{
    int i;
    static struct manufacture_id_t
    {
	int id;
	char *name;
    } manufacture_id_table[] =
    {
	{0x00, "Undefined"},
	{0x01, "Sequential Circuits"},
#if 0
	{0x02, "Big Briar"},
#else
	{0x02, "IDP"},
#endif
	{0x03, "Voyetra/Octave Plateau"},
	{0x04, "Moog Music"},
	{0x05, "Passport Designs"},
	{0x06, "Lexicon"},
	{0x07, "Kurzweil"},
	{0x08, "Fender"},
	{0x09, "Gulbransen"},
	{0x0A, "AKG Acoustics"},
	{0x0B, "Voyce Music"},
	{0x0C, "Waveframe"},
	{0x0D, "ADA"},
	{0x0E, "Garfield Electronics"},
	{0x0F, "Ensoniq"},
	{0x10, "Oberheim"},
	{0x11, "Apple Computer"},
	{0x12, "Grey Matter"},
	{0x13, "Digidesign"},
	{0x14, "Palm Tree Instruments"},
	{0x15, "J L Cooper"},
	{0x16, "Lowrey"},
	{0x17, "Adams-Smith"},
	{0x18, "E-mu Systems"},
	{0x19, "Harmony Systems"},
	{0x1A, "ART"},
	{0x1B, "Baldwin"},
	{0x1C, "Eventide"},
	{0x1D, "Inventronics"},
	{0x1E, "Key Concepts"},
	{0x1F, "Clarity"},
	{0x20, "Passac"},
	{0x21, "SIEL"},
	{0x22, "Synthaxe"},
	{0x23, "Stepp"},
	{0x24, "Hohner"},
	{0x25, "Twister"},
	{0x26, "Solton"},
	{0x27, "Jellinghaus"},
	{0x28, "Southworth"},
	{0x29, "PPG"},
	{0x2A, "JEN"},
	{0x2B, "Solid State Logic"},
	{0x2C, "Audio Veritrieb"},
	{0x2D, "Hinton Instruments"},
	{0x2E, "Soundtracs"},
	{0x2F, "Elka"},
	{0x30, "Dynacord"},
	{0x33, "Clavia Digital Instruments"},
	{0x34, "Audio Architecture"},
	{0x39, "Soundcraft Electronics"},
	{0x3B, "Wersi"},
	{0x3C, "Avab Electronik"},
	{0x3D, "Digigram"},
	{0x3E, "Waldorf Electronics"},
	{0x40, "Kawai"},

/* GS */
#ifdef MID_OFFICIAL_NAME
	{0x41, "Roland"},
#else
	{0x41, "GS"},
#endif

	{0x42, "Korg"},

/* XG */
#if MID_OFFICIAL_NAME
	{0x43, "Yamaha"},
#else
	{0x43, "XG"},
#endif

	{0x44, "Casio"},
	{0x45, "Moridaira"},
	{0x46, "Kamiya"},
	{0x47, "Akai"},
	{0x48, "Japan Victor"},
	{0x49, "Meisosha"},
	{0x4A, "Hoshino Gakki"},
	{0x4B, "Fujitsu Electric"},
	{0x4C, "Sony"},
	{0x4D, "Nishin Onpa"},
	{0x4E, "TEAC"},
	{0x50, "Matsushita Electric"},
	{0x51, "Fostex"},
	{0x52, "Zoom"},
	{0x53, "Midori Electronics"},
	{0x54, "Matsushita Communication Industrial"},
	{0x55, "Suzuki Musical Instrument Mfg."},
	{0x7D, "Non-Commercial"},
/* GM */
#if MID_OFFICIAL_NAME
	{0x7E, "Non-Realtime"},
#else
	{0x7E, "GM"},
#endif

	{0x7F, "Realtime"},
	{-1, NULL}
    };

    if(mid == 0)
	return NULL;
    for(i = 1; manufacture_id_table[i].id != -1; i++)
	if(mid == manufacture_id_table[i].id)
        return manufacture_id_table[i].name;
    return NULL;
}

const char* default_instrum_name_list[] = 
{
    // 钢琴
    "Acoustic Grand Piano 大钢琴(声学钢琴)",
    "Bright Acoustic Piano 明亮的钢琴",
    "Electric Grand Piano 电钢琴",
    "Honky-tonk Piano 酒吧钢琴",
    "Rhodes Piano 柔和的电钢琴",
    "Chorused Piano 加合唱效果的电钢琴",
    "Harpsichord 羽管键琴(拨弦古钢琴)",
    "Clavichord 科拉维科特琴(击弦古钢琴)",
    //色彩打击乐器
    "Celesta 钢片琴",
    "Glockenspiel 钟琴(键盘式钟琴)",
    "Music box 八音盒",
    "Vibraphone 颤音琴",
    "Marimba 马林巴",
    "Xylophone 木琴",
    "Tubular Bells 管钟",
    "Dulcimer 大扬琴",
    //风琴
    "Hammond Organ 击杆风琴",
    "Percussive Organ 打击式风琴",
    "Rock Organ 摇滚风琴",
    "Church Organ 教堂风琴",
    "Reed Organ 簧管风琴",
    "Accordian 手风琴",
    "Harmonica 口琴",
    "Tango Accordian 探戈手风琴",
    //吉他
    "Acoustic Guitar (nylon) 尼龙弦吉他",
    "Acoustic Guitar (steel) 钢弦吉他",
    "Electric Guitar (jazz) 爵士电吉他",
    "Electric Guitar (clean) 清音电吉他",
    "Electric Guitar (muted) 闷音电吉他",
    "Overdriven Guitar 加驱动效果的电吉他",
    "Distortion Guitar 加失真效果的电吉他",
    "Guitar Harmonics 吉他和音",
    //贝司
    "Acoustic Bass 大贝司(声学贝司)",
    "Electric Bass(finger) 电贝司(指弹)",
    "Electric Bass (pick) 电贝司(拨片)",
    "Fretless Bass 无品贝司",
    "Slap Bass 1 掌击Bass 1",
    "Slap Bass 2 掌击Bass 2",
    "Synth Bass 1 电子合成Bass 1",
    "Synth Bass 2 电子合成Bass 2",
    //弦乐
    "Violin 小提琴",
    "Viola 中提琴",
    "Cello 大提琴",
    "Contrabass 低音大提琴",
    "Tremolo Strings 弦乐群颤音音色",
    "Pizzicato Strings 弦乐群拨弦音色",
    "Orchestral Harp 竖琴",
    "Timpani 定音鼓",
    //合奏/合唱
    "String Ensemble 1 弦乐合奏音色1",
    "String Ensemble 2 弦乐合奏音色2",
    "Synth Strings 1 合成弦乐合奏音色1",
    "Synth Strings 2 合成弦乐合奏音色2",
    "Choir Aahs 人声合唱“啊”",
    "Voice Oohs 人声“嘟”",
    "Synth Voice 合成人声",
    "Orchestra Hit 管弦乐敲击齐奏",
    //铜管
    "Trumpet 小号",
    "Trombone 长号",
    "Tuba 大号",
    "Muted Trumpet 加弱音器小号",
    "French Horn 法国号(圆号)",
    "Brass Section 铜管组(铜管乐器合奏音色)",
    "Synth Brass 1 合成铜管音色1",
    "Synth Brass 2 合成铜管音色2",
    //簧管
    "Soprano Sax 高音萨克斯风",
    "Alto Sax 次中音萨克斯风",
    "Tenor Sax 中音萨克斯风",
    "Baritone Sax 低音萨克斯风",
    "Oboe 双簧管",
    "English Horn 英国管",
    "Bassoon 巴松(大管)",
    "Clarinet 单簧管(黑管)",
    //笛
    "Piccolo 短笛",
    "Flute 长笛",
    "Recorder 竖笛",
    "Pan Flute 排箫",
    "Bottle Blow 吹瓶口",
    "Shakuhachi 日本尺八",
    "Whistle 口哨声",
    "Ocarina 奥卡雷那",
    //合成主音
    "Lead 1 (square) 合成主音1(方波)",
    "Lead 2 (sawtooth) 合成主音2(锯齿波)",
    "Lead 3 (caliope lead) 合成主音3",
    "Lead 4 (chiff lead) 合成主音4",
    "Lead 5 (charang) 合成主音5",
    "Lead 6 (voice) 合成主音6(人声)",
    "Lead 7 (fifths) 合成主音7(平行五度)",
    "Lead 8 (bass+lead)合成主音8(贝司加主音)",
    //合成音色
    "Pad 1 (new age) 合成音色1(新世纪)",
    "Pad 2 (warm) 合成音色2(温暖)",
    "Pad 3 (polysynth) 合成音色3",
    "Pad 4 (choir) 合成音色4(合唱)",
    "Pad 5 (bowed) 合成音色5",
    "Pad 6 (metallic) 合成音色6(金属声)",
    "Pad 7 (halo) 合成音色7(光环)",
    "Pad 8 (sweep) 合成音色8",
    //合成效果
    "FX 1 (rain) 合成效果 1 雨声",
    "FX 2 (soundtrack) 合成效果 2 音轨",
    "FX 3 (crystal) 合成效果 3 水晶",
    "FX 4 (atmosphere) 合成效果 4 大气",
    "FX 5 (brightness) 合成效果 5 明亮",
    "FX 6 (goblins) 合成效果 6 鬼怪",
    "FX 7 (echoes) 合成效果 7 回声",
    "FX 8 (sci-fi) 合成效果 8 科幻",
    //民间乐器
    "Sitar 西塔尔(印度)",
    "Banjo 班卓琴(美洲)",
    "Shamisen 三昧线(日本)",
    "Koto 十三弦筝(日本)",
    "Kalimba 卡林巴",
    "Bagpipe 风笛",
    "Fiddle 民族提琴",
    "Shanai 山奈",
    //打击乐器
    "Tinkle Bell 叮当铃",
    "Agogo 阿哥哥铃",
    "Steel Drums 钢鼓",
    "Woodblock 木鱼",
    "Taiko Drum 太鼓",
    "Melodic Tom 通通鼓",
    "Synth Drum 合成鼓",
    "Reverse Cymbal 铜钹",
    //Sound Effects 声音效果
    "Guitar Fret Noise 吉他换把杂音",
    "Breath Noise 呼吸声",
    "Seashore 海浪声",
    "Bird Tweet 鸟鸣",
    "Telephone Ring 电话铃",
    "Helicopter 直升机",
    "Applause 鼓掌声",
    "Gunshot 枪声",
};
