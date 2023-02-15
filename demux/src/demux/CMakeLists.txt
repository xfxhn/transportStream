






add_library(demux demux.cpp)


target_include_directories(demux
        PRIVATE ${PROJECT_SOURCE_DIR}/src/bitStream
        PRIVATE ${PROJECT_SOURCE_DIR}/src/transportStream
        )


target_link_libraries(demux PRIVATE bitStream transportStream)