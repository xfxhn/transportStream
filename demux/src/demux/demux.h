
#ifndef TS_DEMUX_H
#define TS_DEMUX_H

#include <fstream>
#include <cstdint>

class Demux {
private:
    std::ifstream fs;
    std::ofstream videoFs;
    std::ofstream audioFs;
public:

    int init(const char *filename);


    int start();

    ~Demux();
};


#endif //TS_DEMUX_H
