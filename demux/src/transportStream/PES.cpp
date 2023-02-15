
#include "PES.h"
#include <cstdio>
#include "readStream.h"


enum {
    program_stream_map = 0xBC,
    padding_stream = 0xBE,
    private_stream_2 = 0xBF,
    ECM_STREAM = 0xF0,
    EMM_STREAM = 0xF1,
    program_stream_directory = 0xFF,
    DSMCC_stream = 0xF2,
    E_STREAM = 0xF8
};

int PES::PES_packet(ReadStream &rs) {

    packet_start_code_prefix = rs.readMultiBit(24);

    /*
     * stream_id指定基本流的类型和编号，由表2-22中的stream_id定义。
     * 在传输流中，stream_id可以设置为正确描述表2-22中定义的基本流类型的任何有效值。
     * 在传输流中，基本流类型在2.4.4中指定的特定于程序的信息中指定。
     * 对于符合Rec. ITU-T H.264 | ISO/IEC 14496-10附录G中定义的一个或多个配置文件的AVC视频流，
     * 同一AVC视频流的所有视频子比特流应具有相同的stream_id值。
     * 对于符合Rec. ITU-T H.264 | ISO/IEC 14496-10附录H中定义的一个或多个配置文件的AVC视频流，
     * 同一AVC视频流的所有MVC视频流应具有相同的stream_id值。
     * 对于符合Rec. ITU-T H.264 | ISO/IEC 14496-10附录I中定义的一个或多个配置文件的AVC视频流，
     * 同一AVC视频流的所有MVCD视频流子比特流应具有相同的stream_id值。*/
    stream_id = rs.readMultiBit(8);

    /*
     * PES_packet_length顾名思义就是PES包的长度，但是注意，它是2个字节存储的，这意味着，最大只能表示65535，
     * 一旦视频帧很大，超过这个长度，怎么办，就把PES_packet_length置为0，这是ISO标准规定的。
     * 所以在解析的时候，不能以PES_packet_length为准，要参考payload_unit_start_indicator。
*/
    PES_packet_length = rs.readMultiBit(16);
    if (stream_id != program_stream_map
        && stream_id != padding_stream
        && stream_id != private_stream_2
        && stream_id != ECM_STREAM
        && stream_id != EMM_STREAM
        && stream_id != program_stream_directory
        && stream_id != DSMCC_stream
        && stream_id != E_STREAM) {
        rs.readMultiBit(2);
        PES_scrambling_control = rs.readMultiBit(2);
        PES_priority = rs.readMultiBit(1);
        data_alignment_indicator = rs.readMultiBit(1);
        copyright = rs.readMultiBit(1);
        original_or_copy = rs.readMultiBit(1);
        PTS_DTS_flags = rs.readMultiBit(2);
        ESCR_flag = rs.readMultiBit(1);
        ES_rate_flag = rs.readMultiBit(1);
        DSM_trick_mode_flag = rs.readMultiBit(1);
        additional_copy_info_flag = rs.readMultiBit(1);
        PES_CRC_flag = rs.readMultiBit(1);
        PES_extension_flag = rs.readMultiBit(1);
        PES_header_data_length = rs.readMultiBit(8);

        uint8_t N = PES_header_data_length;
        if (PTS_DTS_flags == 2) {
            rs.readMultiBit(4);
            uint8_t pts32_30 = rs.readMultiBit(3);
            rs.readBit();
            uint16_t pts29_15 = rs.readMultiBit(15);
            rs.readBit();
            uint16_t pts14_0 = rs.readMultiBit(15);
            rs.readBit();
            pts = (pts32_30 << 30) | (pts29_15 << 15) | pts14_0;
            /*fprintf(stdout, "pts = %d\n", pts);*/
            N -= 5;
        }

        if (PTS_DTS_flags == 3) {
            rs.readMultiBit(4);
            uint8_t pts32_30 = rs.readMultiBit(3);
            rs.readBit();
            uint16_t pts29_15 = rs.readMultiBit(15);
            rs.readBit();
            uint16_t pts14_0 = rs.readMultiBit(15);
            rs.readBit();
            pts = (pts32_30 << 30) | (pts29_15 << 15) | pts14_0;
            //fprintf(stdout, "pts = %d\n", pts);
            rs.readMultiBit(4);
            uint8_t dts32_30 = rs.readMultiBit(3);
            rs.readBit();
            uint16_t dts29_15 = rs.readMultiBit(15);
            rs.readBit();
            uint16_t dts14_0 = rs.readMultiBit(15);
            rs.readBit();
            dts = (dts32_30 << 30) | (dts29_15 << 15) | dts14_0;
            fprintf(stdout, "pts = %f   dts = %f\n", (double) pts / 90000.0, (double) dts / 90000.0);
            N -= 10;
        }

        if (ESCR_flag == 1) {
            rs.readMultiBit(2);
            uint8_t ESCR_base = rs.readMultiBit(3);
            rs.readBit();
            uint16_t ESCR_base29_15 = rs.readMultiBit(15);
            rs.readBit();
            uint16_t ESCR_base14_0 = rs.readMultiBit(15);
            rs.readBit();
            uint16_t ESCR_extension = rs.readMultiBit(9);
            rs.readBit();

            N -= 6;
        }
        if (ES_rate_flag == 1) {
            rs.readBit();
            uint32_t ES_rate = rs.readMultiBit(22);
            rs.readBit();
            N -= 3;
        }

        if (DSM_trick_mode_flag == 1) {
            uint8_t trick_mode_control = rs.readMultiBit(3);
            if (trick_mode_control == 0) { //Fast forward
                uint8_t field_id = rs.readMultiBit(2);
                uint8_t intra_slice_refresh = rs.readMultiBit(1);
                uint8_t frequency_truncation = rs.readMultiBit(2);
            } else if (trick_mode_control == 1) {//slow_motion
                uint8_t rep_cntrl = rs.readMultiBit(5);
            } else if (trick_mode_control == 2) {//freeze_frame
                uint8_t field_id = rs.readMultiBit(2);
                rs.readMultiBit(3);
            } else if (trick_mode_control == 3) {//fast_reverse
                uint8_t field_id = rs.readMultiBit(2);
                uint8_t intra_slice_refresh = rs.readMultiBit(1);
                uint8_t frequency_truncation = rs.readMultiBit(2);
            } else if (trick_mode_control == 4) {//slow_reverse
                uint8_t rep_cntrl = rs.readMultiBit(5);
            } else {
                rs.readMultiBit(5);
            }
            N -= 1;
        }

        if (additional_copy_info_flag == 1) {
            rs.readBit();
            uint32_t additional_copy_info = rs.readMultiBit(7);
            N -= 1;
        }

        if (PES_CRC_flag == 1) {
            uint8_t previous_PES_packet_CRC = rs.readMultiBit(16);
            N -= 2;
        }
        if (PES_extension_flag == 1) {
            fprintf(stderr, "不支持解析PES_extension_flag\n");
            return -1;
        }
        rs.setBytePtr(N);

    } else if (stream_id == program_stream_map
               || stream_id == private_stream_2
               || stream_id == ECM_STREAM
               || stream_id == EMM_STREAM
               || stream_id == program_stream_directory
               || stream_id == DSMCC_stream
               || stream_id == E_STREAM) {
        fprintf(stderr, "stream_id = %d\n", stream_id);
        return -1;
    } else if (stream_id == padding_stream) {
        /*剩下的填充字节*/
        fprintf(stderr, "没有PES头，剩下的都是填充\n");
        return 0;
    }
    return 0;
}


int PES::writeData(ReadStream &rs, std::ofstream &videoFs) {

    videoFs.write(reinterpret_cast<char *>(rs.currentPtr), rs.size - rs.position);

    return 0;
}
