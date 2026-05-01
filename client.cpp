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
int deviceId = -1;
int totalDevices = 1;
bool isSurround = false;
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
    deviceId = msg.timestamps[0];
    totalDevices = msg.timestamps[1];
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
    deviceId = msg.timestamps[0];
    totalDevices = msg.timestamps[1];
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
    
    string newSongName(msg.data);
    if (songName != newSongName) {
        songName = newSongName;
        if (!downloadSong(songName)) {
            printf("[ERROR] Could not download song, aborting playback\n");
            return;
        }
        string fullPath = filePath("/music/") + songName;
        loadSong(fullPath);
        printf("[UPDATE] Now playing: %s\n", songName.c_str());
    }
    ll now = getServerTime(clientClock);
    ll scheduleServerTime = msg.timestamps[0];
    ll diff = scheduleServerTime - now;
    ll sample_pos;
    if (diff > 0) {
        waitUntil(scheduleServerTime, clientClock);
        return handlePlay(msg);
    } else {
        printf("[EVENT] Resuming\n");
        ll late_ns = -(scheduleServerTime - getServerTime(clientClock));
        sample_pos = msg.timestamps[1] + (late_ns * SAMPLE_RATE) / 1000000000LL;
    }

    if (sample_pos < 0) sample_pos = 0;
    playFromSample(sample_pos);
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

void handleSurround(Message msg){
    if (msg.timestamps[0] == -1){
        setVolume((float)1);
        printf("\n[EVENT] Surround sound off\n");
        isSurround = false; 
        return;
    }
    printf("\n[EVENT] Surround sound on, Seq No: %d\n", deviceId);
    isSurround = true;
}


void surroundController() {
    ll prev = 0; 

    while (true) {
        if (!isSurround) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        ll currentTime = getTime();
        if (currentTime - prev >= SURROUND_CHANGE_GAP) {
            
            double timeComponent = (double)getServerTime(clientClock) / (double)UNIT_SECOND;
            
            double phaseOffset = (double)deviceId * 2.0 * M_PI / (double)totalDevices;
            
            float volume = (float)((std::sin(timeComponent + phaseOffset) + 1.0) / 2.0);
            
            setVolume(volume);
            prev = currentTime;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
    thread t2(surroundController);
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
        }else if (msg.type == MessageType::SURROUND){
            handleSurround(msg);
        }
        if (recvSyncCount >= JOIN_SYNC_COUNT && hasJoined && !isStateFetched){
            printf("Synced with server\n");
            isStateFetched = true;
            askForState();
        }
    }
    t1.join();
    t2.join();
    close(socketfd);
    cleanupAudio();
    return 0;
}