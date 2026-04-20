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

void initAudio(const char* file) {
    if (ma_engine_init(NULL, &engine) != MA_SUCCESS) {
        printf("[ERROR] Failed to init audio engine\n");
        return;
    }
    if (ma_sound_init_from_file(&engine, file, 0, NULL, NULL, &sound) != MA_SUCCESS) {
        printf("[ERROR] Failed to load file: %s\n", file);
        return;
    }
    printf("[AUDIO] Loaded: %s\n", file);
}

void playFromSample(ll sample_pos) {
    ma_sound_seek_to_pcm_frame(&sound, sample_pos);
    ma_sound_start(&sound);
}

void cleanupAudio() {
    ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);
}

#endif