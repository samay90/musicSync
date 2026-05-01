#ifndef TYPES
#define TYPES

#include <bits/stdc++.h>
#include "define.hpp"
using namespace std;

using ll = long long;

enum MessageType {
    JOIN,
    SYNC,
    PLAY,
    PAUSE,
    STATE,
    SURROUND,
    LEAVE
};

struct Message {
    MessageType type;
    char data[BUFFER_SIZE];
    int uid;
    ll timestamps[4];
};

struct Device{
    ll ip;
    ll port;
    int uid;
    char name[BUFFER_SIZE];
};

struct Time{
    ll minRTT;
    ll offset;
    Time(){
        minRTT = LLONG_MAX;
        offset = 0;
    }
    bool operator<(const Time& t) const {
        return minRTT < t.minRTT;
    }
    bool operator>(const Time& t) const {
        return minRTT > t.minRTT;
    }
};

struct MusicState{
    char name[BUFFER_SIZE];
    ll position;
    ll timeStamp;
    bool isPlaying;
};

#endif