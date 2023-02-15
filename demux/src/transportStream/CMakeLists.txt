add_library(transportStream OBJECT transportPacket.cpp SI.cpp PES.cpp)


target_include_directories(transportStream
        PRIVATE ${PROJECT_SOURCE_DIR}/src/bitStream
        )