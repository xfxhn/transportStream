



add_subdirectory(bitStream)
add_subdirectory(nalu)
add_subdirectory(adts)
add_subdirectory(transportStream)
add_subdirectory(mux)


add_executable(ts main.cpp)

target_include_directories(ts PRIVATE ${PROJECT_SOURCE_DIR}/src/mux)
target_link_libraries(ts PRIVATE mux)
