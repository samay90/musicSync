#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <csignal>
#include "lib/types.hpp"
#include "lib/functions.hpp"
#include "lib/define.hpp"
#include "lib/player.hpp"
#include "fileServer/client.hpp"
using namespace std;

int uid, socketfd;
sockaddr_in address;
bool hasJoined = false;
int syncCount = 0;
int recvSyncCount = 0;
bool isStateFetched = false;
string songName;
mutex sendMutex;

Time clientClock;

void sendPacket(Message msg){
    if (!hasJoined && msg.type != MessageType::JOIN) return;
    lock_guard<mutex> lock(sendMutex);
    sendto(socketfd, &msg, sizeof(msg), 0, (sockaddr*)&address, sizeof(address));
}

void sync(){
    Message msg;
    msg.type = MessageType::SYNC;
    msg.uid = uid;
    msg.timestamps[0] = getTime();
    sendPacket(msg);
}

void askForState(){
    Message msg;
    msg.type = MessageType::STATE;
    msg.uid = uid;
    sendPacket(msg);
}


void joinRoom(){
    printf("Enter your name: ");
    string name;
    cin >> name;
    printf("Joining room...\n");
    Message msg;
    msg.type = MessageType::JOIN;
    msg.uid = uid;
    strcpy(msg.data, name.c_str());
    sendPacket(msg);
}

void handleJoin(Message msg){
    if (msg.uid != uid){
        printf("[JOIN] %s joined the room\n", msg.data);
    }else{
        hasJoined = true;
        printf("----------- Welcome %s #%d -----------\n\n", msg.data, uid);
    }
}

void leaveRoom(int sig){
    Message msg;
    msg.type = MessageType::LEAVE;
    msg.uid = uid;
    strcpy(msg.data, "");
    sendPacket(msg);
    cout << endl;
    exit(0);
}

void handleLeave(Message msg){
    if (msg.uid != -1){
        printf("[LEAVE] %s left the room\n", msg.data);
    }else{
        printf("\n----------- Stream ended -----------\n");
        exit(0);
    }
}

void handleSync(Message msg){
    ll t1 = msg.timestamps[0];
    ll t2 = msg.timestamps[1];
    ll t3 = msg.timestamps[2];
    ll t4 = msg.timestamps[3];
    Time t;
    t.minRTT = (t4 - t1) - (t3 - t2);
    t.offset = ((t2 - t1) + (t3 - t4)) / 2;
    clientClock = min(clientClock, t);
    if (LOG){
        printf("[SYNC] minRTT: %lld, offset: %lld\n", clientClock.minRTT, clientClock.offset);
    }
    recvSyncCount++;
}

void handlePlay(Message msg) {
    ma_sound_stop(&sound);
    ll now = getServerTime(clientClock);
    ll scheduleServerTime = msg.timestamps[0];
    ll diff = scheduleServerTime - now;
    string newSongName(msg.data);
    if (songName != newSongName){
        songName = newSongName;
        if (!downloadSong(songName)) {
            printf("[ERROR] Could not download song, aborting playback\n");
            return;
        }
        newSongName = filePath("/music/") + newSongName;
        loadSong(newSongName);
        printf("[UPDATE] Now playing: %s\n", songName.c_str());
    }else{
        printf("[EVENT] Playing\n");
    }
    if (diff > 0) {
        waitUntil(scheduleServerTime, clientClock);
        ll actualNow = getServerTime(clientClock);
        ll late_ns = actualNow - scheduleServerTime;
        ll late_samples = (late_ns * SAMPLE_RATE) / 1000000000LL;
        ll sample_pos = msg.timestamps[1] + late_samples;
        if (sample_pos < 0) sample_pos = 0;
        playFromSample(sample_pos);

    } else {
        ll late_ns = -diff;
        ll sample_pos = msg.timestamps[1] + (late_ns * SAMPLE_RATE) / 1000000000LL;
        if (sample_pos < 0) sample_pos = 0;
        playFromSample(sample_pos);
    }
}

void handlePause(){
    printf("\n[EVENT] Paused\n");
    ma_sound_stop(&sound);
}

void autoSync(){
    ll prev = INT_MIN;
    while (1){
        ll now = getTime();
        if (now - prev > UNIT_SECOND * SYNC_INTERVAL || syncCount < JOIN_SYNC_COUNT){
            sync();
            prev = now;
            syncCount++;
        }
    }
}


int main(){
    srand(time(0));
    uid = rand();
    initAudio();
    signal(SIGINT, leaveRoom);
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(SERVER_IP);
    address.sin_port = htons(PORT);
    socklen_t len = sizeof(address);
    joinRoom();
    thread t1(autoSync);
    while (1){
        Message msg;
        recvfrom(socketfd, &msg, sizeof(msg), 0, (sockaddr*)&address, &len);
        ll now = getTime();
        if (msg.type == MessageType::JOIN){
            handleJoin(msg);
        }else if (msg.type == MessageType::LEAVE){
            handleLeave(msg);
        }else if (msg.type == MessageType::SYNC){
            msg.timestamps[3] = now;
            handleSync(msg);
        }else if (msg.type == MessageType::PLAY){
            thread t2(handlePlay, msg);
            t2.detach();
        }else if (msg.type == MessageType::PAUSE){
            handlePause();
        }
        if (recvSyncCount >= JOIN_SYNC_COUNT && hasJoined && !isStateFetched){
            printf("Synced with server\n");
            isStateFetched = true;
            askForState();
        }
    }
    t1.join();
    close(socketfd);
    cleanupAudio();
    return 0;
}