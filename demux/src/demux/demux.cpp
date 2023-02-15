
#include "demux.h"

#include "readStream.h"
#include "transportPacket.h"

#define TRANSPORT_STREAM_PACKETS_SIZE 188

int Demux::init(const char *filename) {

    fs.open(filename, std::ios::out | std::ios::binary);
    if (!fs.is_open()) {
        fprintf(stderr, "open %s failed\n", filename);
        return -1;
    }


    videoFs.open("resource/test.h264", std::ios::binary | std::ios::out | std::ios::trunc);
    if (!videoFs.is_open()) {
        fprintf(stderr, "open file failed\n");
        return -1;
    }


    audioFs.open("resource/test.aac", std::ios::binary | std::ios::out | std::ios::trunc);
    if (!audioFs.is_open()) {
        fprintf(stderr, "open file failed\n");
        return -1;
    }
    return 0;
}

int Demux::start() {
    int ret;
    uint8_t buffer[TRANSPORT_STREAM_PACKETS_SIZE];
    TransportPacket ts;


    int i = 0;
    uint32_t filesize = 0;
    while (true) {
        fs.read(reinterpret_cast<char *>(buffer), TRANSPORT_STREAM_PACKETS_SIZE);
        uint32_t size = fs.gcount();

        filesize += size;
        if (size != TRANSPORT_STREAM_PACKETS_SIZE) {
            fprintf(stderr, "size = %d\n", size);
            break;
        }
        ReadStream rs(buffer, TRANSPORT_STREAM_PACKETS_SIZE);


        ret = ts.transport_packet(rs, videoFs, audioFs, i);
        if (ret < 0) {
            fprintf(stderr, "transport_packet失败\n");
            return ret;
        }
        ++i;

    }
    //fs.read(reinterpret_cast<char *>(buffer), TRANSPORT_STREAM_PACKETS_SIZE);

    return 0;
}

Demux::~Demux() {
    fs.close();
    videoFs.close();
    audioFs.close();
}
