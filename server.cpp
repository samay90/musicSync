#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <csignal>
#include "lib/types.hpp"
#include "lib/functions.hpp"
#include "lib/define.hpp"
using namespace std;

int socketfd;
map<int, Device> users;

MusicState musicState;

void sendPacket(Message msg, sockaddr_in c_addr){
    sendto(socketfd, &msg, sizeof(msg), 0, (sockaddr*)&c_addr, sizeof(c_addr));
}

void broadcast(Message msg){
    for (auto &[_, device] : users){
        sockaddr_in c_addr;
        c_addr.sin_family = AF_INET;
        c_addr.sin_addr.s_addr = device.ip;
        c_addr.sin_port = device.port;
        sendPacket(msg, c_addr);
    }
}

void sendState(){
    Message msg;
    msg.type = musicState.isPlaying ? MessageType::PLAY : MessageType::PAUSE;
    msg.timestamps[0] = musicState.timeStamp;
    msg.timestamps[1] = musicState.position;
    broadcast(msg);
}

void handleJoin(Message msg, sockaddr_in c_addr){
    Device device;
    device.ip = c_addr.sin_addr.s_addr;
    device.port = c_addr.sin_port;
    device.uid = msg.uid;
    strcpy(device.name, msg.data);
    users[msg.uid] = device;
    printf("[JOIN] %s joined the room\n", msg.data);
    Message brd_msg;
    brd_msg.type = MessageType::JOIN;
    strcpy(brd_msg.data, msg.data);
    brd_msg.uid = msg.uid;
    broadcast(brd_msg);
}

void leaveRoom(Message msg){
    printf("[LEAVE] %s left the room\n", users[msg.uid].name);
    Message brd_msg;
    brd_msg.type = MessageType::LEAVE;
    strcpy(brd_msg.data, users[msg.uid].name);
    brd_msg.uid = msg.uid;
    users.erase(msg.uid);
    broadcast(brd_msg);
}

void handleExit(int sig){
    Message msg;
    msg.type = MessageType::LEAVE;
    msg.uid = -1;
    strcpy(msg.data, "");
    broadcast(msg);
    cout << endl;
    exit(0);
}

void handleSync(Message msg){
    if (!users.count(msg.uid)){
        return;
    }
    Device device = users[msg.uid];
    sockaddr_in c_addr;
    c_addr.sin_family = AF_INET;
    c_addr.sin_addr.s_addr = device.ip;
    c_addr.sin_port = device.port;
    msg.timestamps[2] = getTime();
    sendPacket(msg, c_addr);
}

void handlePlay(){
    printf("\nPlaying...\n");
    ll now = getTime();
    Message msg;
    msg.type = MessageType::PLAY;
    msg.uid = -1;
    msg.timestamps[0] = now + UNIT_SECOND;
    msg.timestamps[1] = musicState.position;
    musicState.isPlaying = true;
    musicState.timeStamp = msg.timestamps[0];
    broadcast(msg);
}

void handlePause(){
    printf("\nPausing...\n");
    if (!musicState.isPlaying){
        return;
    }
    ll now = getTime();
    ll elapsed_ns = now - musicState.timeStamp;
    musicState.position += (elapsed_ns * SAMPLE_RATE) / 1000000000LL;
    musicState.isPlaying = false;
    musicState.timeStamp = now;
    Message msg;
    msg.type = MessageType::PAUSE;
    broadcast(msg);
}
void listner(){
    while (1){
        Message msg;
        sockaddr_in c_addr;
        socklen_t len = sizeof(c_addr);
        recvfrom(socketfd, &msg, sizeof(msg), 0, (sockaddr*)&c_addr, &len);
        ll now = getTime();
        if(msg.type == MessageType::JOIN){
            handleJoin(msg, c_addr);
        }else if (msg.type == MessageType::LEAVE){
            leaveRoom(msg);
        }else if (msg.type == MessageType::SYNC){
            msg.timestamps[1] = now;
            handleSync(msg);
        }else if (msg.type == MessageType::STATE){
            sendState();
        }
    }
}

void printInstructions(){
    cout << "1. JOIN\n2. SYNC\n3. PLAY\n4. PAUSE" << endl;
}


int main(){
    musicState.isPlaying = false;
    musicState.position = 0;
    musicState.timeStamp = getTime();
    strcpy(musicState.name, "song.mp3");

    signal(SIGINT, handleExit);
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    bind(socketfd, (struct sockaddr*)&address, sizeof(address));
    thread t1(listner);
    printInstructions();
    while (1){
        char buffer;
        cin >> buffer;
        if (buffer == '3'){
            handlePlay();
        }else if (buffer == '4'){
            handlePause();
        }
    }
    t1.join();
    close(socketfd);
    return 0;
}