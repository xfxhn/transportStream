


add_library(mux mux.cpp)


target_include_directories(mux
        PRIVATE ${PROJECT_SOURCE_DIR}/src/bitStream
        PUBLIC ${PROJECT_SOURCE_DIR}/src/nalu
        PUBLIC ${PROJECT_SOURCE_DIR}/src/adts
        PRIVATE ${PROJECT_SOURCE_DIR}/src/transportStream

        )


target_link_libraries(mux PRIVATE bitStream nalu adts transportStream)