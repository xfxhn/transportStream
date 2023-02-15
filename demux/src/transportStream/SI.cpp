

#include "SI.h"

#include <cstdio>
#include "readStream.h"

/*具体参考mpeg-1 part1 table 2-34*/
#define AVC 0x1B
#define AAC 0x0F


enum {
    PROGRAM_ASSOCIATION_SECTION = 0x00,
    conditional_access_section = 0x01,
    PROGRAM_MAP_SECTION = 0x02,
    /*实际的网络*/
    network_information_section = 0x40,
    /*其他网络*/
    /*network_information_section_other = 0x41,*/
    /*实际运输流*/
    SERVICE_DESCRIPTION_SECTION = 0x42,
    /*其他运输流*/
    /*service_description_section_other = 0x46,*/
    BOUQUET_ASSOCIATION_SECTION = 0x4A,

    /*后面还有一些table_id对应的一些表，具体参考SI规范 Table 2: Allocation of table_id values*/
};

int SI::analysisTable(ReadStream &rs) {
    table_id = rs.readMultiBit(8);

    switch (table_id) {
        case PROGRAM_ASSOCIATION_SECTION:
            program_association_section(rs);
            break;
        case PROGRAM_MAP_SECTION:
            program_map_section(rs);
            break;
        case SERVICE_DESCRIPTION_SECTION:
            service_description_section(rs);
            break;
        case BOUQUET_ASSOCIATION_SECTION:
            bouquet_association_section(rs);
            break;
        default:
            fprintf(stderr, "暂时不支持解析table_id = %d的值\n", table_id);
            break;
    }


    /*if (table_id == 0x42 || table_id == 0x46) {
        service_description_section(rs);
    }*/
    return 0;
}

int SI::bouquet_association_section(ReadStream &rs) {


    return 0;
}

int SI::service_description_section(ReadStream &rs) {
    sdt.section_syntax_indicator = rs.readMultiBit(1);
    rs.readMultiBit(1);
    rs.readMultiBit(2);
    sdt.section_length = rs.readMultiBit(12);
    sdt.transport_stream_id = rs.readMultiBit(16);
    rs.readMultiBit(2);
    sdt.version_number = rs.readMultiBit(5);
    sdt.current_next_indicator = rs.readMultiBit(1);
    sdt.section_number = rs.readMultiBit(8);
    sdt.last_section_number = rs.readMultiBit(8);
    sdt.original_network_id = rs.readMultiBit(16);
    rs.readMultiBit(8);

    //  for (int i = 0; i < rs.size - rs.position; ++i) {
    sdt.service_id = rs.readMultiBit(16);
    rs.readMultiBit(6);
    sdt.EIT_schedule_flag = rs.readBit();
    sdt.EIT_present_following_flag = rs.readBit();
    sdt.running_status = rs.readMultiBit(3);
    sdt.free_CA_mode = rs.readBit();
    sdt.descriptors_loop_length = rs.readMultiBit(12);

    service_descriptor(rs);
    //  }
    sdt.CRC_32 = rs.readMultiBit(32);
    return 0;
}

int SI::service_descriptor(ReadStream &rs) {
    sdt.descriptor_tag = rs.readMultiBit(8);
    sdt.descriptor_length = rs.readMultiBit(8);
    sdt.service_type = rs.readMultiBit(8);

    uint8_t service_provider_name_length = rs.readMultiBit(8);
    sdt.service_provider_name = new char[service_provider_name_length + 1]{0};
    rs.getString(sdt.service_provider_name, service_provider_name_length);

    uint8_t service_name_length = rs.readMultiBit(8);
    sdt.service_name = new char[service_name_length + 1]{0};
    rs.getString(sdt.service_name, service_name_length);

    return 0;
}


int SI::program_association_section(ReadStream &rs) {
    pat.section_syntax_indicator = rs.readBit();
    rs.readBit();
    rs.readMultiBit(2);
    /*
     * 这是一个12位的字段，前两位是'00'。
     * 其余10位指定section的字节数，从section_length字段后面开始，包括CRC。该字段的值不能超过1021 (0x3FD)。
     * */
    pat.section_length = rs.readMultiBit(12);
    pat.transport_stream_id = rs.readMultiBit(16);
    rs.readMultiBit(2);
    /*5bits版本号码，标注当前节目的版本．这是个非常有用的参数，当检测到这个字段改变时，说明ＴＳ流中的节目已经变化 了，程序必须重新搜索节目*/
    pat.version_number = rs.readMultiBit(5);
    pat.current_next_indicator = rs.readBit();
    /*当前段号码*/
    pat.section_number = rs.readMultiBit(8);
    /*
     * 最后段号码(section_number和 last_section_number的功能是当PAT内容>184字节时，
     * PAT表会分成多个段(sections),解复用程序必须在全部接 收完成后再进行PAT的分析)
     * */
    pat.last_section_number = rs.readMultiBit(8);


    int N = (pat.section_length - 9) / 4;
    /*
     * 从for()开始，就是描述了当前流中的频道数目(N),每一个频道对应的PMT PID是什么
     * N是一个变量，计算方法是N=(section_length-9)/4
     * */
    /*每个 TS 流对应一张，用来描述该 TS 流中有多少个节目*/
    for (int i = 0; i < N; ++i) {
        /*
         * Program_number是一个16位字段。它指定program_map_PID适用于的程序。
         * 当设置为0x0000时，下面的PID引用将是网络PID。
         * 对于所有其他情况，此字段的值是用户定义的。
         * 在程序关联表的一个版本中，该字段不得使用任何单一值超过一次。
         * */
        /*该号码标志ＴＳ流中的一个频道，该频道可以包含很多的节目(即可以包含多个Video PID和Audio PID) */
        uint16_t program_number = rs.readMultiBit(16);
        rs.readMultiBit(3);
        if (program_number == 0) {
            /*
             * network_PID是一个13位的字段，它只与设置为0x0000的program_number值一起使用，指定传输流数据包的PID，其中应包含网络信息表。
             * network_PID字段的值由用户定义，但只能取表2-3中规定的值。
             * network_PID的存在是可选的。
             * */
            pat.network_PID = rs.readMultiBit(13);

            fprintf(stderr, "网络节目号 = %d\n", pat.network_PID);
        } else {
            Program program;
            program.program_number = program_number;
            /*
             * program_map_PID是一个13位的字段，用于指定传输流包的PID，其中应包含program_number指定的适用于程序的program_map_section。
             * program_number不能有多个program_map_PID赋值。program_map_PID的值由用户定义，但只能取表2-3中规定的值。
             * */
            /*表示本频道使用的哪个PID做为PMT的ＰＩＤ，因为可以有很多的频道，因此ＤＶＢ规定PMT的PID可以由用户自己定义*/
            program.program_map_PID = rs.readMultiBit(13);

            pat.list.push_back(program);
        }
    }

    /*本段的CRC校验值，一般是会忽略的*/
    pat.CRC_32 = rs.readMultiBit(32);
    patFlag = true;
    return 0;
}

/*如果 TS 流中包含多个节目，那么就会有多个 PMT 表*/
int SI::program_map_section(ReadStream &rs) {
    pmt.section_syntax_indicator = rs.readBit();
    rs.readBit();
    rs.readMultiBit(2);
    pmt.section_length = rs.readMultiBit(12);
    uint16_t N = pmt.section_length;
    /*频道号码,表示当前的PMT关联到的频道.换句话就是说,当前描述的是program_number频道的信息*/
    pmt.program_number = rs.readMultiBit(16);
    rs.readMultiBit(2);
    /*版本号码,如果PMT内容有更新,则version_number会递增1通知解复用程序需要重新接收节目信息,否则 version_number是固定不变的*/
    pmt.version_number = rs.readMultiBit(5);
    pmt.current_next_indicator = rs.readBit();
    pmt.section_number = rs.readMultiBit(8);
    pmt.last_section_number = rs.readMultiBit(8);
    rs.readMultiBit(3);
    pmt.PCR_PID = rs.readMultiBit(13);
    rs.readMultiBit(4);
    uint16_t program_info_length = rs.readMultiBit(12);
    rs.setBytePtr(program_info_length);
    /*for (int i = 0; i < program_info_length; ++i) {
        description()
    }*/
    N = N - 9 - program_info_length - 4;
    while (N > 0) {

        /*流类型，标志是Video还是Audio还是其他数据，h.264编码对应0x1b，aac编码对应0x0f，mp3编码对应0x03*/
        uint8_t stream_type = rs.readMultiBit(8);
        rs.readMultiBit(3);
        uint16_t elementary_PID = rs.readMultiBit(13);
        rs.readMultiBit(4);
        uint16_t ES_info_length = rs.readMultiBit(12);
        rs.setBytePtr(ES_info_length);
        /*for (int i = 0; i < ES_info_length; ++i) {
            description()
        }*/
        if (stream_type == AVC) {
            pmt.videoPid = elementary_PID;
        } else if (stream_type == AAC) {
            pmt.audioPid = elementary_PID;
        } else {
            printf("不是AVC，也不是AAC，具体查看标准文档mpeg-1 part1 table 2-34\n");
        }
        N -= 5 - ES_info_length;
    }


    pmt.CRC_32 = rs.readMultiBit(32);
    pmtFlag = true;
    return 0;
}

SI::~SI() {
    delete[] sdt.service_provider_name;
    delete[] sdt.service_name;
}







