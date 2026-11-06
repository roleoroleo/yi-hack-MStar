/*
 * Copyright (c) 2026 roleo.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Read h264 content from a pipe and send it to live555.
 */

#ifndef _R_RTSP_SERVER_H
#define _R_RTSP_SERVER_H

//#define _GNU_SOURCE

#define RESOLUTION_NONE 0
#define RESOLUTION_LOW  360
#define RESOLUTION_HIGH 1080
#define RESOLUTION_BOTH 1440

#define OUTPUT_BUFFER_SIZE_LOW  131072
#define OUTPUT_BUFFER_SIZE_HIGH 524288
#define OUTPUT_BUFFER_SIZE_AUDIO 32768

#define CODEC_NONE -1
#define CODEC_H264 0
#define CODEC_H265 1

#define SEND_BUFFER_TO 128                   // 128 KB buffer

#include "RTSPServer.hh"
#include "GroupsockHelper.hh"

class rRTSPServer: public RTSPServer {
public:
    static rRTSPServer* createNew(UsageEnvironment& env, Port ourPort,
                                  UserAuthenticationDatabase* authDatabase,
                                  unsigned reclamationSeconds = 65) {
        int ourSocketIPv4 = setUpOurSocket(env, ourPort, AF_INET);
        int ourSocketIPv6 = setUpOurSocket(env, ourPort, AF_INET6);
        if (ourSocketIPv4 < 0 && ourSocketIPv6 < 0) return NULL;

        return new rRTSPServer(env, ourSocketIPv4, ourSocketIPv6, ourPort, authDatabase, reclamationSeconds);
    }

protected:
    rRTSPServer(UsageEnvironment& env, int ourSocketIPv4, int ourSocketIPv6, Port ourPort,
                UserAuthenticationDatabase* authDatabase, unsigned reclamationSeconds)
        : RTSPServer(env, ourSocketIPv4, ourSocketIPv6, ourPort, authDatabase, reclamationSeconds) {
    }

    virtual ClientConnection* createNewClientConnection(int clientSocket, struct sockaddr_storage const& clientAddr) {
        // RTP over TCP is sent on this socket: give it more room than the default 50 KB
        increaseSendBufferTo(envir(), clientSocket, SEND_BUFFER_TO*1024);
        return RTSPServer::createNewClientConnection(clientSocket, clientAddr);
    }
};

long long current_timestamp();

#endif
