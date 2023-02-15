

#include "demux.h"

int main() {
    int ret;
    Demux ts;
/*video_1671610380-00001.ts*/
/*test.ts*/
    ret = ts.init("./resource/test0.ts");
    if (ret < 0) {
        fprintf(stderr, "初始化失败\n");
        return -1;
    }
    ts.start();
    return 0;
}

