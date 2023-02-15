
#include "mux.h"


int main() {
    int ret;
    Mux mux;
    ret = mux.init("mux/resource/test.ts");
    if (ret < 0) {
        fprintf(stderr, "init failed\n");
        return ret;
    }
    ret = mux.initVideo("mux/resource/ouput.h264");
    if (ret < 0) {
        fprintf(stderr, "initVideo failed\n");
        return ret;
    }
    ret = mux.initAudio("mux/resource/ouput.aac");
    if (ret < 0) {
        fprintf(stderr, "initAudio failed\n");
        return ret;
    }


    ret = mux.start();
    if (ret < 0) {
        fprintf(stderr, "封装失败\n");
        return 0;
    }
    return 0;
}
