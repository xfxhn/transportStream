
#include "mux.h"

#include <string>
#include "readStream.h"
#include "writeStream.h"

#include "transportPacket.h"

#define TRANSPORT_STREAM_PACKETS_SIZE 188
enum NalUintType {
    H264_NAL_UNSPECIFIED = 0,
    H264_NAL_SLICE = 1,
    H264_NAL_IDR_SLICE = 5,
    H264_NAL_SEI = 6,
    H264_NAL_SPS = 7,
    H264_NAL_PPS = 8
};

int Mux::init(const char *filename) {
    fs.open("mux/resource/bbb/test0.ts", std::ios::binary | std::ios::out | std::ios::trunc);
    if (!fs.is_open()) {
        fprintf(stderr, "cloud not open %s\n", filename);
        return -1;
    }
    buffer = new uint8_t[TRANSPORT_STREAM_PACKETS_SIZE];

    return 0;
}

int Mux::initVideo(const char *filename) {
    int ret;
    ret = videoReader.init(filename);
    if (ret < 0) {
        fprintf(stderr, "init video failed\n");
        return ret;
    }
    return ret;
}

int Mux::initAudio(const char *filename) {
    int ret;
    ret = audioReader.init(filename);
    if (ret < 0) {
        fprintf(stderr, "init audio failed\n");
        return ret;
    }
    return ret;
}


int Mux::start() {
    int ret;
    WriteStream ws(buffer, TRANSPORT_STREAM_PACKETS_SIZE);
    /*首先写入PSI信息*/
    TransportPacket ts;

    writeTable(ts, ws);


    bool videoFlag = true;
    bool audioFlag = true;
    NALPicture *picture = gop.dpb[0];
    AdtsHeader header;

    int num = 0;
    std::string name = "mux/resource/bbb/test";
    while (videoFlag || audioFlag) {

        /*如果有视频，但是没音频，就算视频pts大于音频pts也要去取视频*/
        if (videoFlag && (!audioFlag || picture->pts <= header.pts)) {
            ret = getVideoFrame(picture, videoFlag);
            if (ret < 0) {
                return ret;
            }
            /*获取到视频是idr帧的时候才会去切片*/
            if (picture->duration >= 10 && picture->sliceHeader.nalu.IdrPicFlag) {
                fs.close();
                fs.open(name + std::to_string(++num) + ".ts", std::ios::binary | std::ios::out | std::ios::trunc);
                if (!fs.is_open()) {
                    fprintf(stderr, "cloud not open %s\n", (name + std::to_string(num) + ".ts").c_str());
                    return -1;
                }
                writeTable(ts, ws);
            }
            ret = ts.writeVideoFrame(picture, ws, fs);
            if (ret < 0) {
                fprintf(stderr, "写入视频帧失败\n");
                return ret;
            }
        } else {
            ret = getAudioFrame(header, audioFlag);
            if (ret < 0) {
                return ret;
            }

            ret = ts.writeAudioFrame(header, ws, fs);
            if (ret < 0) {
                fprintf(stderr, "写入音频帧失败\n");
                return ret;
            }
        }

        /*printf("video duration = %f -- audio duration = %f -- idr = %d\n", picture->duration, header.duration,
               picture->sliceHeader.nalu.IdrPicFlag);*/
    }
    return 0;
}

int Mux::getAudioFrame(AdtsHeader &header, bool &flag) {
    int ret;
    ret = audioReader.adts_sequence(header, flag);
    if (ret < 0) {
        fprintf(stderr, "解析adts_sequence 失败\n");
        return ret;
    }
    /*(header.sample_rate / 1024.0) 计算一帧多少秒*/
    header.duration += (1024.0 / header.sample_rate);
    header.dts = av_rescale_q(audioDecodeFrameNumber, {1, static_cast<int>(header.sample_rate)}, {1, 1000});
    header.pts = header.dts;
    audioDecodeFrameNumber += 1024;
    return 0;
}

void Mux::writeTable(TransportPacket &ts, WriteStream &ws) {

    ts.writeServiceDescriptionTable(ws);
    fs.write(reinterpret_cast<const char *>(buffer), TRANSPORT_STREAM_PACKETS_SIZE);

    ts.writeProgramAssociationTable(ws);
    fs.write(reinterpret_cast<const char *>(buffer), TRANSPORT_STREAM_PACKETS_SIZE);

    ts.writeProgramMapTable(ws);
    fs.write(reinterpret_cast<const char *>(buffer), TRANSPORT_STREAM_PACKETS_SIZE);
}

/*取出一个完整的视频帧*/
int Mux::getVideoFrame(NALPicture *&picture, bool &flag) {
    int ret;
    if (finishFlag) {
        flag = false;
        /*计算pts和dts*/
        computedTimestamp(unoccupiedPicture);
        picture = unoccupiedPicture;
        return 0;
    }

    if (picture->useFlag) {
        /*把当前这个picture标记为不在使用，可以释放*/
        picture->useFlag = false;
        picture = unoccupiedPicture;
    }

    uint8_t *data = nullptr;
    uint32_t size = 0;
    int startCodeLength = 0;

    while (flag) {
        /*每次读取一个nalu*/
        ret = videoReader.readNalUint(data, size, startCodeLength, flag);
        if (ret < 0) {
            fprintf(stderr, "读取nal uint发生错误\n");
            return ret;
        }

        /*读取nalu header*/
        nalUnitHeader.nal_unit(data[0]);

        if (nalUnitHeader.nal_unit_type == H264_NAL_SPS) {
            const uint32_t tempSize = size + startCodeLength;
            uint8_t *buf = new uint8_t[tempSize]; // NOLINT(modernize-use-auto)
            memcpy(buf, data - startCodeLength, tempSize);

            /*去除防竞争字节*/
            NALHeader::ebsp_to_rbsp(data, size);
            ReadStream rs(data, size);

            NALSeqParameterSet sps;
            sps.seq_parameter_set_data(rs);
            spsList[sps.seq_parameter_set_id] = sps;

            if (picture->useFlag) {
                /*计算pts和dts*/
                computedTimestamp(picture);
                /*
                   * 参考图片标记过程
                   * 当前图片的所有SLICE都被解码。
                   * 参考8.2.5.1第一条规则
                   * */
                /*这里标记了这个使用的帧是长期参考还是短期参考，并且给出一个空闲的帧*/
                ret = gop.decoding_finish(picture, unoccupiedPicture);
                if (ret < 0) {
                    fprintf(stderr, "图像解析失败\n");
                    return ret;
                }

                unoccupiedPicture->size += tempSize;
                unoccupiedPicture->data.push_back({tempSize, buf, 0});
                return 0;
            }

            picture->size += tempSize;
            picture->data.push_back({tempSize, buf, 1});

        } else if (nalUnitHeader.nal_unit_type == H264_NAL_PPS) {
            const uint32_t tempSize = size + startCodeLength;
            uint8_t *buf = new uint8_t[tempSize]; // NOLINT(modernize-use-auto)
            memcpy(buf, data - startCodeLength, tempSize);


            NALHeader::ebsp_to_rbsp(data, size);
            ReadStream rs(data, size);

            NALPictureParameterSet pps;
            pps.pic_parameter_set_rbsp(rs, spsList);
            ppsList[pps.pic_parameter_set_id] = pps;
            if (picture->useFlag) {
                computedTimestamp(picture);
                /*
                    * 参考图片标记过程
                    * 当前图片的所有SLICE都被解码。
                    * 参考8.2.5.1第一条规则
                    * */
                /*这里标记了这个使用的帧是长期参考还是短期参考，并且给出一个空闲的帧*/
                ret = gop.decoding_finish(picture, unoccupiedPicture);
                if (ret < 0) {
                    fprintf(stderr, "图像解析失败\n");
                    return ret;
                }

                unoccupiedPicture->size += tempSize;
                unoccupiedPicture->data.push_back({tempSize, buf, 0});
                return 0;
            }

            picture->size += tempSize;
            picture->data.push_back({tempSize, buf, 2});
        } else if (nalUnitHeader.nal_unit_type == H264_NAL_SLICE || nalUnitHeader.nal_unit_type == H264_NAL_IDR_SLICE) {
            const uint32_t tempSize = size + startCodeLength;
            uint8_t *buf = new uint8_t[tempSize]; // NOLINT(modernize-use-auto)
            memcpy(buf, data - startCodeLength, tempSize);

            NALHeader::ebsp_to_rbsp(data, size);
            ReadStream rs(data, size);

            NALSliceHeader sliceHeader;
            sliceHeader.slice_header(rs, nalUnitHeader, spsList, ppsList);

            /*先处理上一帧，如果tag有数据并且first_mb_in_slice=0表示有上一针*/
            if (sliceHeader.first_mb_in_slice == 0 && picture->useFlag) {
                /*计算pts和dts*/
                computedTimestamp(picture);

                /*
                    * 参考图片标记过程
                    * 当前图片的所有SLICE都被解码。
                    * 参考8.2.5.1第一条规则
                    * */
                /*这里标记了这个使用的帧是长期参考还是短期参考，并且给出一个空闲的帧*/
                ret = gop.decoding_finish(picture, unoccupiedPicture);
                if (ret < 0) {
                    fprintf(stderr, "图像解析失败\n");
                    return ret;
                }
                /*设置这个空闲的帧的slice header*/
                unoccupiedPicture->sliceHeader = sliceHeader;
                unoccupiedPicture->useFlag = true;

                /*解码POC*/
                unoccupiedPicture->decoding_process_for_picture_order_count();

                /*参考帧重排序在每个P、SP或B片的解码过程开始时调用。*/
                if (sliceHeader.slice_type == H264_SLIECE_TYPE_P ||
                    sliceHeader.slice_type == H264_SLIECE_TYPE_SP ||
                    sliceHeader.slice_type == H264_SLIECE_TYPE_B) {
                    gop.decoding_process_for_reference_picture_lists_construction(unoccupiedPicture);
                }
                unoccupiedPicture->size += tempSize;
                unoccupiedPicture->data.push_back({tempSize, buf, 0});
                /*所有数据获取完毕*/
                if (!flag) {
                    /*最后一帧*/
                    finishFlag = true;
                    flag = true;
                }

                return 0;
            }

            picture->sliceHeader = sliceHeader;
            if (sliceHeader.first_mb_in_slice == 0) {
                /*表示这帧正在使用，不要释放*/
                picture->useFlag = true;

                /*解码POC*/
                picture->decoding_process_for_picture_order_count();

                /*参考帧重排序在每个P、SP或B片的解码过程开始时调用。*/
                if (sliceHeader.slice_type == H264_SLIECE_TYPE_P ||
                    sliceHeader.slice_type == H264_SLIECE_TYPE_SP ||
                    sliceHeader.slice_type == H264_SLIECE_TYPE_B) {
                    gop.decoding_process_for_reference_picture_lists_construction(picture);
                }
            }
            picture->size += tempSize;
            picture->data.push_back({tempSize, buf, 0});
        } else if (nalUnitHeader.nal_unit_type == H264_NAL_SEI) {
            //++i;
        } else {
            fprintf(stderr, "不支持解析 nal_unit_type = %d\n", nalUnitHeader.nal_unit_type);
        }
    }
    return 0;
}


void Mux::computedTimestamp(NALPicture *picture) {


    if (picture->pictureOrderCount == 0) {
        videoDecodeIdrFrameNumber = videoDecodeFrameNumber * 2;
    }

    picture->pts = av_rescale_q((videoDecodeIdrFrameNumber + picture->pictureOrderCount) / 2,
                                picture->sliceHeader.sps.timeBase,
                                {1, 1000});
    picture->dts = av_rescale_q(videoDecodeFrameNumber, picture->sliceHeader.sps.timeBase, {1, 1000});
    picture->pcr = av_rescale_q(videoDecodeFrameNumber, picture->sliceHeader.sps.timeBase, {1, 1000});


    ++videoDecodeFrameNumber;
    picture->duration = (double) videoDecodeFrameNumber / picture->sliceHeader.sps.fps;
    /* printf("pts = %d    dts = %d   poc = %d\n", picture->pts, picture->dts, picture->pictureOrderCount);*/


}


Mux::~Mux() {
    delete[] buffer;

    fs.close();
}


uint64_t Mux::av_rescale_q(uint64_t a, const AVRational &bq, const AVRational &cq) {
    //(1 / 25) / (1 / 1000);
    int64_t b = bq.num * cq.den;
    int64_t c = cq.num * bq.den;
    return a * b / c;  //25 * (1000 / 25)  把1000分成25份，然后当前占1000的多少
}







