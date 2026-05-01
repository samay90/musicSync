#ifndef PLAYER
#define PLAYER

#include <bits/stdc++.h>
#include "define.hpp"
using namespace std;

#define MINIAUDIO_IMPLEMENTATION
#include "../modules/miniaudio.h"
#include "define.hpp"
#include "types.hpp"

ma_engine engine;
ma_sound  sound;

void initAudio() {
    ma_engine_init(NULL, &engine);
}

void playFromSample(ll sample_pos) {
    ma_sound_seek_to_pcm_frame(&sound, sample_pos);
    ma_sound_start(&sound);
}

void cleanupAudio() {
    ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);
}

void loadSong(const string& path){
    ma_sound_stop(&sound);
    ma_sound_uninit(&sound);
    if (ma_sound_init_from_file(&engine, path.c_str(), 0, NULL, NULL, &sound) != MA_SUCCESS){
        printf("[ERROR] Failed to load: %s\n", path.c_str());
    }
}

void setVolume(float volume) {
    ma_sound_set_volume(&sound, volume);
}

#endif