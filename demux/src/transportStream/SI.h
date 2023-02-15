

#ifndef TS_SI_H
#define TS_SI_H

#include <cstdint>
#include <vector>

class ReadStream;


struct SDT {
    uint8_t section_syntax_indicator{0};
    /*它指定section的字节数，从紧接着section_length字段开始，包括CRC*/
    /*section_length不能超过1021，这样整个section的最大长度为1024字节。*/
    uint16_t section_length{0};
    uint16_t transport_stream_id{0};
    uint8_t version_number{0};
    uint8_t current_next_indicator{0};
    uint8_t section_number{0};
    uint8_t last_section_number{0};
    uint16_t original_network_id{0};
    uint16_t service_id{0};
    uint8_t EIT_schedule_flag{0};
    uint8_t EIT_present_following_flag{0};
    uint8_t running_status{0};
    uint8_t free_CA_mode{0};
    uint8_t descriptors_loop_length{0};
    uint8_t CRC_32{0};


    uint8_t descriptor_tag{0};
    uint8_t descriptor_length{0};
    uint8_t service_type{0};
    char *service_provider_name{nullptr};
    char *service_name{nullptr};
};
struct Program {
    uint16_t program_number{0};
    uint16_t program_map_PID{0};
};
struct PAT {
    uint8_t table_id{0};
    uint8_t section_syntax_indicator{0};
    /*它指定section的字节数，从紧接着section_length字段开始，包括CRC*/
    /*section_length不能超过1021，这样整个section的最大长度为1024字节。*/
    uint16_t section_length{0};
    uint16_t transport_stream_id{0};
    uint8_t version_number{0};
    uint8_t current_next_indicator{0};
    uint8_t section_number{0};
    uint8_t last_section_number{0};

    /*uint16_t program_number{0};*/
    uint16_t network_PID{0};
    /*uint16_t program_map_PID{0};*/
    std::vector<Program> list;

    uint32_t CRC_32{0};
};

struct PMT {
    uint8_t section_syntax_indicator{0};
    uint16_t section_length{0};
    uint16_t program_number{0};
    uint8_t version_number{0};
    uint8_t current_next_indicator{0};
    uint8_t section_number{0};
    uint8_t last_section_number{0};
    uint16_t PCR_PID{0};


    uint32_t CRC_32{0};


    uint16_t videoPid{0};
    uint16_t audioPid{0};
};

class SI {
public:
    PAT pat;
    SDT sdt;
    PMT pmt;

    bool patFlag{false};
    bool pmtFlag{false};
private:
    uint8_t table_id{0};
public:
    int analysisTable(ReadStream &rs);

    ~SI();

private:
    int service_description_section(ReadStream &rs);

    int service_descriptor(ReadStream &rs);

    int program_association_section(ReadStream &rs);

    int program_map_section(ReadStream &rs);

    int bouquet_association_section(ReadStream &rs);


};


#endif //TS_SI_H
