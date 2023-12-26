
#ifndef NEW_PLAYER_FFPLAYER_H
#define NEW_PLAYER_FFPLAYER_H

#include "JNICallbakcHelper.h"
#include "safe_queue.h"
#include "util.h"

extern "C" { // ffmpeg是纯c写的，必须采用c的编译方式，否则奔溃
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
};

typedef void(*RenderCallback)(uint8_t *, int, int, int); // 函数指针声明定义  // TODO 第三节课新增

class FFPlayer {

private :
    char *data_source = 0; // 指针 请赋初始值
    JNICallbakcHelper *jniCallbackHelper = 0;
    RenderCallback renderCallback = 0;
    pthread_t pid_prepare = 0;
    pthread_t pid_video_decode = 0;
    pthread_t pid_start = 0;
    pthread_t pid_video_play = 0;
    AVFormatContext *formatContext = 0; // 媒体上下文 封装格式
    pthread_t pid_stop = 0;

    bool isStreaming = 0; // 是否播放
    AVCodecContext *codecContext = 0; // 音频 视频 都需要的 解码器上下文

public:

    SafeQueue<AVPacket *> packets; // 压缩的 数据包   AudioChannel.cpp(packets 1)   VideoChannel.cpp(packets 2)
    SafeQueue<AVFrame *> frames; // 原始的

    FFPlayer(const char *data_source, JNICallbakcHelper *helper);

    ~FFPlayer();

    void setRenderCallback(RenderCallback renderCallback);

    void prepare();

    void get_media_info();

    void stop();

    void stop_streaming(FFPlayer *derryPlayer);

    void start_streaming();


    void get_stream_process();

    static void releaseAVPacket(AVPacket **p) {
        if (p) {
            av_packet_free(p); // 释放队列里面的 T == AVPacket
            *p = 0;
        }
    }

    static void releaseAVFrame(AVFrame **f) {
        if (f) {
            av_frame_free(f); // 释放队列里面的 T == AVFrame
            *f = 0;
        }
    }

    void start_decode_thread();

    void start_play_thread();

    void video_decode();

    void video_play();
};


#endif //NEW_PLAYER_FFPLAYER_H
