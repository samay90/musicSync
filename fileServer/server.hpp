#include "../lib/functions.hpp"
#include "../lib/define.hpp"
#include "../lib/types.hpp"
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "bits/stdc++.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;

void tcpFileServer() {
    int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(TCP_PORT);
    
    bind(tcpfd, (sockaddr*)&addr, sizeof(addr));
    listen(tcpfd, 10);
    printf("[TCP] File server listening on port %d\n", TCP_PORT);
    
    while (1) {
        sockaddr_in client_addr = {};
        socklen_t len = sizeof(client_addr);
        int connfd = accept(tcpfd, (sockaddr*)&client_addr, &len);
        if (connfd < 0) continue;

        char filename[256] = {};
        if (!recvExact(connfd, filename, sizeof(filename))) {
            close(connfd);
            continue;
        }

        if (strstr(filename, "..") || strchr(filename, '/') || strchr(filename, '\\')) {
            printf("[TCP] Rejected unsafe filename: %s\n", filename);
            off_t zero = 0;
            write(connfd, &zero, sizeof(zero));
            close(connfd);
            continue;
        }

        off_t resumeFrom = 0;
        if (!recvExact(connfd, &resumeFrom, sizeof(resumeFrom))) {
            close(connfd);
            continue;
        }

        string path = filePath("/music/") + string(filename);
        int filefd = open(path.c_str(), O_RDONLY);

        if (filefd < 0) {
            off_t zero = 0;
            write(connfd, &zero, sizeof(zero));
            close(connfd);
            printf("[TCP] File not found: %s\n", filename);
            continue;
        }

        struct stat st;
        fstat(filefd, &st);
        off_t fileSize = st.st_size;

        if (resumeFrom < 0 || resumeFrom >= fileSize) resumeFrom = 0;

        write(connfd, &fileSize, sizeof(fileSize));

        off_t offset = resumeFrom;
        while (offset < fileSize) {
            ssize_t sent = sendfile(connfd, filefd, &offset, fileSize - offset);
            if (sent <= 0) {
                if (errno == EINTR || errno == EAGAIN) continue;
                printf("[ERROR] Client disconnected during transfer\n");
                break;
            }
        }
        
        close(filefd);
        close(connfd);
        printf("[TCP] Served: %s (%ld/%ld bytes)\n", filename, (long)offset, (long)fileSize);
    }
}