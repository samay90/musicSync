#ifndef FUNCTIONS
#define FUNCTIONS

#include <bits/stdc++.h>
#include "define.hpp"
using namespace std;

inline ll getTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ll)ts.tv_sec * 1000000000LL + (ll)ts.tv_nsec;
}

ll getServerTime(Time clk) {
    return getTime() + clk.offset;
}

void waitUntil(ll targetTime, Time clk) {
    ll diff = targetTime - getServerTime(clk);

    if (diff > 2000000LL)
        this_thread::sleep_for(chrono::nanoseconds(diff - 2000000LL));

    while (getServerTime(clk) < targetTime);

}

string filePath(string filename) {
    char path[BUFFER_SIZE];
    getcwd(path, sizeof(path));
    return string(path) + "/" + filename;
}

#endif