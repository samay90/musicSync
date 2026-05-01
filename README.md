# musicSync

`musicSync` is a lightweight C++ project for synchronized audio playback across multiple clients. It combines a UDP-based sync protocol, a TCP file server for song downloads, and local audio playback using `miniaudio`.

## Features

- Server-client synchronization using UDP messages
- NTP-style clock synchronization to align client playback timing
- Scheduled playback so all clients start audio in sync
- TCP-backed song download with resume support
- Command-line server controls for play, pause, seek, and surround sound
- Multi-device surround audio effect using device-specific sine-wave volume control
- Uses local `music/` directory for available songs

## Architecture

- `server.cpp` - UDP server for client discovery, sync, playback commands, and state broadcasting
- `client.cpp` - UDP client that joins the room, syncs time, downloads songs, and plays audio
- `fileServer/server.hpp` - TCP file server that serves MP3 files from `music/`
- `fileServer/client.hpp` - TCP client used by the client app to download songs
- `lib/` - shared helper code for time, audio, networking, and message definitions
- `modules/miniaudio.h` - single-header audio playback library included in the repo

## Requirements

- Linux
- `g++`
- `make`
- Audio output device

## Build

From the repository root:

```bash
make
```

This creates two binaries:

- `server`
- `client`

## Usage

1. Place audio files in the `music/` directory.
   - Supported files should be accessible by `miniaudio` (typically MP3/WAV).

2. Start the server:

```bash
./server
```

3. Start one or more clients:

```bash
./client
```

4. On each client, enter a display name when prompted.

5. Use the server console to control playback:

- `1` - List available songs
- `2` - Select a song by number
- `3` - Play the current song
- `4` - Pause playback
- `5` - Seek +10 seconds
- `6` - Seek -10 seconds
- `7` - Turn on surround sound
- `8` - Turn off surround sound
- `?` - Show instructions

## Network settings

- UDP port: `8888`
- TCP port: `8080`
- Server address for file downloads is configured in `lib/define.hpp` via `SERVER_IP`

If the server runs on a different machine, update `SERVER_IP` in `lib/define.hpp` before building.

## Notes

- The server uses `music/` relative to the repository root.
- Clients automatically sync with the server several times after joining.
- The sync logic is similar to NTP: clients exchange timestamps with the server, estimate round-trip time and clock offset, then adjust playback timing.
- The surround mode uses each client’s device ID to compute a phase offset in a sine wave. That sine wave is used as a dynamic volume controller to create a spatial 3D sound effect across devices.
- The TCP file download client caches completed downloads in `music/` and resumes interrupted transfers.

## Clean

```bash
make clean
```

