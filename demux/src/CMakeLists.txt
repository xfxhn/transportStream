


add_subdirectory(bitStream)
add_subdirectory(demux)
add_subdirectory(transportStream)

add_executable(ts main.cpp)


target_include_directories(ts
        PRIVATE ${PROJECT_SOURCE_DIR}/src/demux
        )
target_link_libraries(ts PRIVATE demux)