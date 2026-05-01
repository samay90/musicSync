#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <csignal>
#include <sys/sendfile.h>
#include <fcntl.h>
#include "lib/types.hpp"
#include "lib/functions.hpp"
#include "lib/define.hpp"
#include "fileServer/server.hpp"
using namespace std;

int socketfd;
map<int, Device> users;

ll DELAY_TIME = UNIT_SECOND;

MusicState musicState;

void sendPacket(Message msg, sockaddr_in c_addr){
    sendto(socketfd, &msg, sizeof(msg), 0, (sockaddr*)&c_addr, sizeof(c_addr));
}

void broadcast(Message msg, bool sendDeviceID = false){
    int cnt = 0;
    for (auto &[_, device] : users){
        sockaddr_in c_addr;
        c_addr.sin_family = AF_INET;
        c_addr.sin_addr.s_addr = device.ip;
        c_addr.sin_port = device.port;
        if (sendDeviceID){
            msg.timestamps[0] = cnt++;
            msg.timestamps[1] = users.size();
        }
        sendPacket(msg, c_addr);
    }
}

void sendState(){
    Message msg;
    msg.type = musicState.isPlaying ? MessageType::PLAY : MessageType::PAUSE;
    msg.timestamps[0] = musicState.timeStamp;
    msg.timestamps[1] = musicState.position;
    strcpy(msg.data, musicState.name);
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
    broadcast(brd_msg, true);
}

void leaveRoom(Message msg){
    printf("[LEAVE] %s left the room\n", users[msg.uid].name);
    Message brd_msg;
    brd_msg.type = MessageType::LEAVE;
    strcpy(brd_msg.data, users[msg.uid].name);
    brd_msg.uid = msg.uid;
    users.erase(msg.uid);
    broadcast(brd_msg, true);
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
    if (musicState.name[0] == '\0'){
        printf("[ERROR] No song selected\n");
        return;
    }
    printf("\n[EVENT] Playing\n");
    ll now = getTime();
    Message msg;
    msg.type = MessageType::PLAY;
    msg.uid = -1;
    strcpy(msg.data, musicState.name);
    musicState.isPlaying = true;
    msg.timestamps[0] = now + UNIT_SECOND;
    msg.timestamps[1] = musicState.position;
    musicState.timeStamp = msg.timestamps[0];
    musicState.position = msg.timestamps[1];
    broadcast(msg);
}

void handlePause(){
    printf("\n[EVENT] Paused\n");
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
    printf("------ Instructions ------\n");
    printf("1. List available songs\n");
    printf("2. Select a song\n");
    printf("3. Play the song\n");
    printf("4. Pause the song\n");
    printf("5. Seek +10 seconds\n");
    printf("6. Seek -10 seconds\n");
    printf("7. Turn on Surround Sound\n");
    printf("8. Turn off Surround Sound\n");
    cout << endl;
}

void listSongs(){
    string songs = runCommand("ls " + filePath("/music/") + " | sed 's/\\.mp3$//' | nl -w1 -s'. '");
    printf("------ Available Songs ------\n");
    cout << songs << endl;
    cout << endl;
}

string getSongName(int number) {
    if (number <= 0){
        printf("[ERROR] Song number must be greater than 0\n");
        return "";
    }
    string cmd = "ls " + filePath("/music/") + " | sed -n '" + to_string(number) + "p' 2>/dev/null";
    string name = runCommand(cmd);
    if (!name.empty() && name.back() == '\n') name.pop_back();
    if (name.empty()){
        printf("[ERROR] No song at number %d\n", number);
        return "";
    }
    return name;
}

void handleChangeMusic(string songString){
    int songNumber;
    try{
        songNumber = stoi(songString);
    }
    catch(const std::exception& e){
        printf("[ERROR] Invalid song number\n");
        return;
    }
    
    string songName = getSongName(songNumber);
    if (songName.empty()){
        return;
    }
    cout << "[UPDATE] Now playing: " << songName << endl;
    musicState.position = 0;
    strcpy(musicState.name, songName.c_str());
    handlePlay();
}

void handleSeek(int offset){
    printf("\n[EVENT] Seek %ds\n", offset);
    ll now = getTime();
    if (musicState.isPlaying){
        ll elapsed_ns = now - musicState.timeStamp;
        musicState.position += (elapsed_ns * SAMPLE_RATE) / 1000000000LL;
    }
    musicState.position += (ll)(offset * SAMPLE_RATE);
    if (musicState.position < 0) musicState.position = 0;
    musicState.timeStamp = now;
    if (musicState.isPlaying){
        Message msg;
        msg.type = MessageType::PLAY;
        msg.uid = -1;
        msg.timestamps[0] = now + UNIT_SECOND;
        msg.timestamps[1] = musicState.position;
        strcpy(msg.data, musicState.name);
        musicState.timeStamp = msg.timestamps[0];
        broadcast(msg);
    }
}

void handleSurround(bool state){
    Message msg;
    msg.type = MessageType::SURROUND;
    msg.uid = -1;
    if (state){
        printf("\n[EVENT] Surround sound on\n");
        msg.timestamps[0] = 1;
    }else{
        printf("\n[EVENT] Surround sound off\n");

        msg.timestamps[0] = -1;
    }
    broadcast(msg);
}



int main(){
    musicState.isPlaying = false;
    musicState.position = 0;
    musicState.timeStamp = -1;
    musicState.name[0] = '\0';

    signal(SIGINT, handleExit);
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    bind(socketfd, (struct sockaddr*)&address, sizeof(address));
    thread t1(listner);
    thread t2(tcpFileServer);
    printInstructions();
    while (1){
        char buffer;
        cin >> buffer;
        if (buffer == '1'){
            listSongs();
        }else if (buffer == '2'){
            cout << "Enter song number: ";
            string song; cin >> song;
            cout << endl;
            handleChangeMusic(song);
        }
        else if (buffer == '3'){
            handlePlay();
        }else if (buffer == '4'){
            handlePause();
        }else if (buffer == '5'){
            handleSeek(10);
        }else if (buffer == '6'){
            handleSeek(-10);
        }else if (buffer == '7'){
            handleSurround(true);
        }else if (buffer == '8'){
            handleSurround(false);
        }else if (buffer == '?'){
            printInstructions();
        }
    }
    t1.join();
    t2.join();
    close(socketfd);
    return 0;
}