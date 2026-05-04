#ifndef FUNCTIONS
#define FUNCTIONS

#include <bits/stdc++.h>
#include "define.hpp"
using namespace std;

ll getTime()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ll)ts.tv_sec * 1000000000LL + (ll)ts.tv_nsec;
}

ll getServerTime(Time clk)
{
    return getTime() + clk.offset;
}

void waitUntil(ll targetTime, Time clk)
{
    ll diff = targetTime - getServerTime(clk);

    if (diff > 2000000LL)
        this_thread::sleep_for(chrono::nanoseconds(diff - 2000000LL));

    while (getServerTime(clk) < targetTime)
        ;
}

string filePath(string filename)
{
    char path[BUFFER_SIZE];
    getcwd(path, sizeof(path));
    return string(path) + "/" + filename;
}

string runCommand(const string &cmd)
{
    FILE *pipe = popen(cmd.c_str(), "r");
    char buffer[256];
    string result;
    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
    {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

bool recvExact(int fd, void *buf, size_t len)
{
    size_t received = 0;
    char *ptr = (char *)buf;
    while (received < len)
    {
        ssize_t n = recv(fd, ptr + received, len - received, 0);
        if (n <= 0)
            return false;
        received += n;
    }
    return true;
}

#endif