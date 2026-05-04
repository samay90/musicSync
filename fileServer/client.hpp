#include "../lib/functions.hpp"
#include "../lib/define.hpp"
#include "../lib/types.hpp"
#include <sys/sendfile.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include "bits/stdc++.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;

bool downloadSong(const string &songName, int maxRetries = 3)
{
    string musicDir = filePath("/music/");
    mkdir(musicDir.c_str(), 0755);

    string localPath = musicDir + songName;
    string tmpPath = localPath + ".tmp";

    if (access(localPath.c_str(), F_OK) == 0)
    {
        printf("[CACHE] Already have: %s\n", songName.c_str());
        return true;
    }

    for (int attempt = 1; attempt <= maxRetries; attempt++)
    {
        if (attempt > 1)
        {
            printf("[RETRY] Attempt %d/%d for: %s\n", attempt, maxRetries, songName.c_str());
        }

        off_t resumeFrom = 0;
        struct stat st;
        if (stat(tmpPath.c_str(), &st) == 0 && st.st_size > 0)
        {
            resumeFrom = st.st_size;
            printf("[RESUME] Found partial file, resuming from byte %ld\n", (long)resumeFrom);
        }

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            perror("[ERROR] socket()");
            sleep(attempt);
            continue;
        }

        int flag = 1, rcvbuf = 262144;
        setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

        struct timeval tv = {.tv_sec = 30, .tv_usec = 0};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(SERVER_IP);
        addr.sin_port = htons(TCP_PORT);

        if (connect(sockfd, (sockaddr *)&addr, sizeof(addr)) < 0)
        {
            printf("[ERROR] Could not connect to file server: %s\n", strerror(errno));
            close(sockfd);
            sleep(attempt);
            continue;
        }

        char filename[256] = {};
        strncpy(filename, songName.c_str(), sizeof(filename) - 1);
        if (write(sockfd, filename, sizeof(filename)) != sizeof(filename))
        {
            printf("[ERROR] Failed to send filename\n");
            close(sockfd);
            sleep(attempt);
            continue;
        }

        if (write(sockfd, &resumeFrom, sizeof(resumeFrom)) != sizeof(resumeFrom))
        {
            printf("[ERROR] Failed to send resume offset\n");
            close(sockfd);
            sleep(attempt);
            continue;
        }

        off_t fileSize = 0;
        if (!recvExact(sockfd, &fileSize, sizeof(fileSize)) || fileSize <= 0)
        {
            printf("[ERROR] Song not found on server: %s\n", songName.c_str());
            close(sockfd);
            return false;
        }

        if (resumeFrom >= fileSize)
        {
            close(sockfd);
            rename(tmpPath.c_str(), localPath.c_str());
            return true;
        }

        printf("[DOWNLOAD] Fetching %s (%ld bytes)... (attempt %d, offset %ld)\n",
               songName.c_str(), (long)fileSize, attempt, (long)resumeFrom);

        int openFlags = O_WRONLY | O_CREAT | ((resumeFrom > 0) ? O_APPEND : O_TRUNC);
        int filefd = open(tmpPath.c_str(), openFlags, 0644);
        if (filefd < 0)
        {
            printf("[ERROR] Could not open temp file: %s — %s\n", tmpPath.c_str(), strerror(errno));
            close(sockfd);
            return false;
        }

        char buf[65536];
        off_t received = resumeFrom;
        bool ok = true;

        while (received < fileSize)
        {
            ssize_t toRead = min((off_t)sizeof(buf), fileSize - received);
            ssize_t n = recv(sockfd, buf, toRead, 0);

            if (n <= 0)
            {
                printf("[ERROR] Connection dropped at %ld/%ld bytes: %s\n",
                       (long)received, (long)fileSize, strerror(errno));
                ok = false;
                break;
            }

            ssize_t written = write(filefd, buf, n);
            if (written != n)
            {
                printf("[ERROR] Disk write failed at %ld bytes: %s\n",
                       (long)received, strerror(errno));
                ok = false;
                break;
            }
            received += n;
        }

        close(filefd);
        close(sockfd);

        if (ok && received == fileSize)
        {
            if (rename(tmpPath.c_str(), localPath.c_str()) == 0)
            {
                printf("[DOWNLOAD] Done: %s\n", songName.c_str());
                return true;
            }
            else
            {
                printf("[ERROR] rename() failed: %s\n", strerror(errno));
                return false;
            }
        }

        printf("[ERROR] Incomplete download: %s (%ld/%ld bytes)\n",
               songName.c_str(), (long)received, (long)fileSize);

        sleep(attempt);
    }

    remove(tmpPath.c_str());
    printf("[ERROR] All %d attempts failed for: %s\n", maxRetries, songName.c_str());
    return false;
}