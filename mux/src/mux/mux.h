
#ifndef MUX_MUX_H
#define MUX_MUX_H

#include <fstream>
#include "NALReader.h"
#include "NALHeader.h"
#include "NALSeqParameterSet.h"
#include "NALPictureParameterSet.h"
#include "NALDecodedPictureBuffer.h"

#include "adtsReader.h"
#include "adtsHeader.h"

class NALPicture;

class TransportPacket;

class WriteStream;

class Mux {

private:
    /*切片数量*/
    int num{0};
    std::string dir;

    NALReader videoReader;
    NALHeader nalUnitHeader;
    NALDecodedPictureBuffer gop;

    uint64_t videoDecodeFrameNumber{0};
    uint64_t videoDecodeIdrFrameNumber{0};
    NALPicture *unoccupiedPicture{nullptr};


    AdtsReader audioReader;
    int64_t audioDecodeFrameNumber{0};


private:
    std::ofstream fs;
    uint8_t *buffer{nullptr};

    NALSeqParameterSet spsList[32];
    NALPictureParameterSet ppsList[256];


    bool finishFlag{false};
public:
    int init(std::string filename);

    int initAudio(const char *filename);

    int initVideo(const char *filename);


    int start();


    ~Mux();

private:
    int getVideoFrame(NALPicture *&picture, bool &flag);

    int getAudioFrame(AdtsHeader &header, bool &flag);

    void writeTable(TransportPacket &ts, WriteStream &ws);

    void computedTimestamp(NALPicture *picture);


    static uint64_t av_rescale_q(uint64_t a, const AVRational &bq, const AVRational &cq);
};


#endif //MUX_MUX_H
