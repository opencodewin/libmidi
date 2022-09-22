<div align="left">
  <img src="images/tanlu-logo.png" height="100" width="100"/>
  <div>&nbsp;</div>
</div>
<div align="left">

</div>
# News
- 2022/9/22  Update libmidi version to 2.15.3, we support very large midi file playing such as millions voices midi files

 [millions voices midi demo](midi_demo/test/red_zone_insane_black_remix.mid)
 
# Introduction
MIDI player base on timidity and imgui, support soundfont(SF2) and software MIDI keyboard.

- **App main window with paino keyboard view**

  ![paino view Image](images/main_window_piano.jpg)

- **App main window with event rolling**

  ![event view Image](images/main_window_events.jpg)

- **Soundfont Select**

  ![soundfont select Image](images/soundfont_select.jpg)

- **Software MIDI keyboard**

  ![midi keyboard Image](images/soft_midi_keyboard.jpg)

  *Software MIDI keyboard mapping* 

  ![midi keyboard mapping](images/soft_midi_keyboard.jpg)

- **Software MIDI keyboard instrument**

  ![midi keyboard instrument Image](images/keboard_mapping.jpg)

- **Console demo**

  ![console demo Image](images/console_demo.jpg)

# Getting the Source

```bash
git clone --recurse-submodules https://github.com/tanluteam/libmidi
```

# Building

This project supports [CMake](https://cmake.org/) out of the box.

- **Linux**

Install dependence library ncurses with apt

- **MacOS**

Install dependence library ncurses with homwbrew

- **Windows**

Using MSYS64 + Mingw64 + Clang/GCC
Install dependence library with pacman

- **imgui build**

See imgui document [imgui](https://github.com/tanluteam/imgui/blob/master/docs/README.md)

### Quick start:

- **download soundfont file**
*[GoogleDrive](https://drive.google.com/drive/folders/1ZwNonqFTQR1dRY0hijSMQbrLkHfJbkvW?usp=sharing)*.
*[BaiduYun](https://pan.baidu.com/s/1FIu6PsjO1FnMSABpSgPK8Q?pwd=901u) code: 901u*.

Copy soundfont files into soundfont folder, then you can using extra soundfont(GM best). if you have SF2 format soundfont files, also put them into folder.

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
./build/immidi
./build/timidity
```

# LICENSE

This project is released under [The MIT License](LICENSE)。
