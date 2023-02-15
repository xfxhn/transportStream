
#ifndef TS_PES_H
#define TS_PES_H

#include <cstdint>
#include <fstream>

class ReadStream;

class PES {
private:
    uint32_t packet_start_code_prefix{0};
    uint8_t stream_id{0};
    uint16_t PES_packet_length{0};


    uint8_t PES_scrambling_control{0};
    uint8_t PES_priority{0};
    uint8_t data_alignment_indicator{0};
    uint8_t copyright{0};
    uint8_t original_or_copy{0};
    uint8_t PTS_DTS_flags{0};
    uint8_t ESCR_flag{0};
    uint8_t ES_rate_flag{0};
    uint8_t DSM_trick_mode_flag{0};
    uint8_t additional_copy_info_flag{0};
    uint8_t PES_CRC_flag{0};
    uint8_t PES_extension_flag{0};
    uint8_t PES_header_data_length{0};


    uint64_t pts{0};
    uint64_t dts{0};
public:
    int PES_packet(ReadStream &rs);

    static int writeData(ReadStream &rs, std::ofstream &videoFs);
};


#endif //TS_PES_H
