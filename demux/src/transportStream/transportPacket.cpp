

#include "transportPacket.h"
#include <cstdio>

#include "readStream.h"
#include "SI.h"


/*SI PID分配*/
enum {
    PAT = 0x0000, //节目关联表
    CAT = 0x0001, //条件访问表
    TSDT = 0x0002,//传输流描述表
    NIT_ST = 0x0010,
    SDT_BAT_ST = 0x0011,
    EIT_ST = 0x0012,
    RST_ST = 0x0013,
    TDT_TOT_ST = 0x0014
};

/*具体参考mpeg-1 part1 table 2-34*//*
#define AVC 0x1B
#define AAC 0x0F*/

static SI info;

/*static PES pes;*/

int TransportPacket::transport_packet(ReadStream &rs, std::ofstream &videoFs, std::ofstream &audioFs, int i) {
    int ret;

    sync_byte = rs.readMultiBit(8);
    if (sync_byte == 255) {
        int aa = 1;
    }
    transport_error_indicator = rs.readMultiBit(1);
    payload_unit_start_indicator = rs.readMultiBit(1);
    transport_priority = rs.readMultiBit(1);
    PID = rs.readMultiBit(13);
    transport_scrambling_control = rs.readMultiBit(2);
    adaptation_field_control = rs.readMultiBit(2);
    continuity_counter = rs.readMultiBit(4);

    /*
     * adaptation_field_control=0,是保留以后使用
     * adaptation_field_control=1，没有adaptation_field，只有有效载荷
     * adaptation_field_control=2，只有adaptation_field，没有有效载荷
     * adaptation_field_control=3，adaptation_field后面跟着有有效载荷
     * */
    if (adaptation_field_control == 2 || adaptation_field_control == 3) {
        ret = adaptation_field(rs);
        if (ret < 0) {
            fprintf(stderr, "解析adaptation_field失败\n");
            return ret;
        }
    }


    /*
     一个 PES 包经过 TS 复用器会拆分成多个定长的 TS 包，那么怎么知道 PES 包从哪个 TS 包开始呢？
     payload_unit_start_indicator 的作用就在此，当 TS 包有效载荷包含 PES 包数据时，payload_unit_start_indicator 具有以下意义：
     1 指示此 TS 包的有效载荷随着 PES 包的首字节开始，
     0 指示在此 TS 包中无任何 PES 包将开始。

     当 TS 包有效载荷包含 PSI 数据时，payload_unit_start_indicator 具有以下意义：
     若 TS 包承载 PSI 分段的首字节，则 payload_unit_start_indicator 值必为 1，
     指示此 TS 包的有效载荷的首字节承载 pointer_field。
     若 TS 包不承载 PSI 分段的首字节，则 payload_unit_start_indicator 值为 0，
     指示在此有效载荷中不存在 pointer_field
*/
    /*
        如果是PES包的话，payload_unit_start_indicator有两个值，0或1，具体的意思我们来举个例子。
        假设有两个视频帧，每个视频帧假设都需要3个ts packet包来存放一个PES包。那么一共有6个ts packet，
        它们的payload_unit_start_indicator的值为1,0,0,1,0,0，值为1代表一个帧的开始，下一个1就是新的一帧的开始了
*/
    /*

     与 PES 包传输一样，通过 TS 包传送 PSI 表时，因为 TS 包的数据负载能力是有限的，
     即每个 TS 包的长度有限，所有当 PSI 表比较大时，PSI 被分成多段（section），再由多个 TS 包传输段。
     每一个段的长度不一，一个段的开始由 TS 包的有效负载中的 payload_unit_start_indicator 来标识
*/

    if (adaptation_field_control == 1 || adaptation_field_control == 3) {
        switch (PID) {
            case PAT:
            case CAT:
            case TSDT:
            case NIT_ST:
            case SDT_BAT_ST:
            case EIT_ST:
            case RST_ST:
            case TDT_TOT_ST:
                /*如果是PSI数据 payload_unit_start_indicator=1，表示有一个pointer_field，调整字节*/
                if (payload_unit_start_indicator) {
                    rs.setBytePtr(1);
                } else {
                    fprintf(stderr, "传输PSI表可能很大，超过188字节，这里没去做处理\n");
                    return -1;
                }
                info.analysisTable(rs);
                break;
            default:
                if (info.patFlag) {
                    /*
                     * 解析出PMT，有可能有很多个节目
                     * info.pat.list存放的是有多少个节目,多少个pmt表
                     * */
                    /*这里只取第一个节目*/
                    const Program &program = info.pat.list[0];
                    if (program.program_map_PID == PID) {
                        if (payload_unit_start_indicator) {
                            rs.setBytePtr(1);
                        } else {
                            fprintf(stderr, "传输PSI表可能很大，超过188字节，这里没去做处理\n");
                            return -1;
                        }
                        /*PMT*/
                        info.analysisTable(rs);
                        return 0;
                    }
                }
                if (info.pmtFlag) {

                    if (info.pmt.videoPid == PID) {
                        /*表示有PES头，也是一个PES包的开始*/
                        if (payload_unit_start_indicator) {
                            ret = pes.PES_packet(rs);
                            if (ret < 0) {
                                fprintf(stderr, "解析PES_packet失败\n");
                                return ret;
                            }
                            PES::writeData(rs, videoFs);
                        } else {
                            PES::writeData(rs, videoFs);
                        }
                    } else if (info.pmt.audioPid == PID) {
                        /*if (payload_unit_start_indicator) {
                            ret = pes.PES_packet(rs);
                            if (ret < 0) {
                                fprintf(stderr, "解析PES_packet失败\n");
                                return ret;
                            }
                            PES::writeData(rs, audioFs);
                        } else {
                            PES::writeData(rs, audioFs);
                        }*/
                    } else {
                        printf("PID = %d\n", PID);
                    }
                }
                break;
        }
    }


    return 0;
}

int TransportPacket::adaptation_field(ReadStream &rs) {


    /*指定紧接在adaptation_field_length后面的adaptation_field的字节数*/
    /*值0表示在传输流数据包中插入单个填充字节*/
    /*当adaptation_field_control值为'11'时，adaptation_field_length的取值范围为0 ~ 182。*/
    /*当adaptation_field_control值为'10'时，adaptation_field_length值为183*/
    /*对于携带PES报文的Transport Stream报文，当PES报文数据不足以完全填充Transport Stream报文有效负载字节时，需要进行填充*/
    adaptation_field_length = rs.readMultiBit(8);

    uint8_t N = adaptation_field_length;
    if (adaptation_field_length > 0) {
        discontinuity_indicator = rs.readBit();
        random_access_indicator = rs.readBit();
        elementary_stream_priority_indicator = rs.readBit();
        PCR_flag = rs.readBit();
        OPCR_flag = rs.readBit();
        splicing_point_flag = rs.readBit();
        transport_private_data_flag = rs.readBit();
        adaptation_field_extension_flag = rs.readBit();
        N -= 1;
        if (PCR_flag) {

            /*Mpeg-2规定的系统时钟频率为27MHz 也就是一秒27MHz*/
            /*
             * system_clock_frequency = 27000000
             * PCR_base(i) = ((system_clock_frequency*t(i)) / 300)%2^33 */
            /*
             * CR_base:以1/300 的系统时钟频率周期为单位，称之为program_clock_reference_base
             * PCR-base的作用:
             * a. 与PTS和DTS作比较, 当二者相同时, 相应的单元被显示或者解码.
             * b. 在解码器切换节目时,提供对解码器PCR计数器的初始值,以让该PCR值与PTS、DTS最大可能地达到相同的时间起点
             * */
            program_clock_reference_base = rs.readMultiBit(33);
            rs.readMultiBit(6);
            /*PCR_ext(i) = ((system_clock_frequency*t(i)) / 1)%300 */
            program_clock_reference_extension = rs.readMultiBit(9);

            /*PCR(i) = PCR_base(i)*300 + PCR_ext(i) */
            uint64_t PCR = program_clock_reference_base * 300 + program_clock_reference_extension;
            printf("PCR = %f\n", (double) PCR / 27000000.0);
            /*PCR 指示包含 program_clock_reference_base 最后一个 bit 的字节在系统目标解码器的输入端的预期到达时间*/
            N -= 6;
        }

        if (OPCR_flag) {
            original_program_clock_reference_base = rs.readMultiBit(33);
            rs.readMultiBit(6);
            original_program_clock_reference_extension = rs.readMultiBit(9);
            N -= 6;
        }
        if (splicing_point_flag) {
            splice_countdown = rs.readMultiBit(8);
            N -= 1;
        }
        if (transport_private_data_flag) {
            transport_private_data_length = rs.readMultiBit(8);
            private_data_byte = new uint8_t[transport_private_data_length];
            for (int i = 0; i < transport_private_data_length; i++) {
                private_data_byte[i] = rs.readMultiBit(8);
            }
            N -= 1 + transport_private_data_length;
        }

        /*当设置为“1”时表示存在一个自适应字段扩展。值为“0”表示自适应字段中不存在自适应字段扩展*/
        if (adaptation_field_extension_flag) {
            fprintf(stderr, "不支持解析自适应字段扩展解析\n");
            return -1;

            adaptation_field_extension_length = rs.readMultiBit(8);
            ltw_flag = rs.readBit();
            piecewise_rate_flag = rs.readBit();
            seamless_splice_flag = rs.readBit();
            rs.readMultiBit(5);
            if (ltw_flag) {
                ltw_valid_flag = rs.readBit();
                ltw_offset = rs.readMultiBit(15);
            }
            if (piecewise_rate_flag) {
                rs.readMultiBit(2);
                piecewise_rate = rs.readMultiBit(22);
            }

            if (seamless_splice_flag) {
                // todo
            }

        }


        rs.setBytePtr(N);
    }


    return 0;
}

