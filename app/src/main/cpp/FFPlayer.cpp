#include "include/FFPlayer.h"

// 构造方法
FFPlayer::FFPlayer(const char *data_source, JNICallbakcHelper *helper) {

    this->data_source = new char[strlen(data_source) + 1];
    strcpy(this->data_source, data_source); // 把源 Copy给成员

    this->jniCallbackHelper = helper;

    this->isStreaming = false;
}

// 析构方法
FFPlayer::~FFPlayer() {

    if (data_source) {
        delete data_source;
        data_source = nullptr;
    }

    if (jniCallbackHelper) {
        delete jniCallbackHelper;
        jniCallbackHelper = nullptr;
    }

    LOGD("~BKCppPlayer");

}


void FFPlayer::setRenderCallback(RenderCallback renderCallback_) {
    this->renderCallback = renderCallback_;
}


void FFPlayer::stop_streaming(FFPlayer *derryPlayer) {

    if (isStreaming) {
        isStreaming = false;
    }

    if (pid_prepare) {
        LOGD("Join pid_prepare");
        pthread_join(pid_prepare, nullptr);
    }

    if (pid_video_decode) {
        LOGD("Join pid_prepare");
        pthread_join(pid_video_decode, nullptr);
    }

    if (pid_start) {
        LOGD("Join pid_start");
        pthread_join(pid_start, nullptr);
    }

    if (pid_video_play) {
        LOGD("Join pid_video_play");
        pthread_join(pid_video_play, nullptr);
    }

    LOGD("Release formatContext");
    // pid_prepare pid_start 就全部停止下来了  稳稳的停下来
    if (formatContext) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }

    // 释放工作
    avcodec_close(codecContext);
    avcodec_free_context(&codecContext);

    DELETE(derryPlayer);
}

void FFPlayer::get_stream_process() {
    // 一股脑 把 AVPacket * 丢到 队列里面去  不区分 音频 视频
    LOGD("get_stream_process");

    while (isStreaming) {
        // 解决方案：视频 我不丢弃数据，等待队列中数据 被消费 内存泄漏点1.1
        if (packets.size() > 10) {
            av_usleep(10 * 1000); // 单位：microseconds 微妙 10毫秒
            // LOGD("video_channel->packets.size() > 10");
            continue;
        }

        // LOGD("av_read_frame");

        // AVPacket 可能是音频 也可能是视频（压缩包）
        AVPacket *packet = av_packet_alloc();
        // 从流到frame
        int ret = av_read_frame(formatContext, packet);
        if (!ret) {
            // 通常stream idx == 0 代表视频
            if (0 == packet->stream_index) {
                // 代表是视频
                // LOGD("Insert video packet to queue");
                if (packets.size() < 10) {
                    packets.insertToQueue(packet);
                } else {
                    av_packet_unref(packet);
                    av_packet_free(&packet);
                }
            }
        } else if (ret == AVERROR_EOF) { //   end of file == 读到文件末尾了 == AVERROR_EOF
            if (packets.empty()) {
                LOGE("Packet is empty error");
                break; // 队列的数据被音频 视频 全部播放完毕了，我再退出
            }
        } else {
            LOGD("av_read_frame error");
            break; // av_read_frame(formatContext,  packet); 出现了错误，结束当前循环
        }
    }

    LOGD("get_stream_process process done");

}

void FFPlayer::get_media_info() { // 属于 子线程了 并且 拥有  DerryPlayer的实例 的 this

    formatContext = avformat_alloc_context();

    // 字典（键值对）
    AVDictionary *dictionary = nullptr;
    //设置超时（5秒）
    av_dict_set(&dictionary, "timeout", "5000000", 0); // 单位微妙

    /**
     * 1，AVFormatContext *
     * 2，路径 url:文件路径或直播地址
     * 3，AVInputFormat *fmt  Mac、Windows 摄像头、麦克风， 我们目前安卓用不到
     * 4，各种设置：例如：Http 连接超时， 打开rtmp的超时  AVDictionary **options
     */
    int r = avformat_open_input(&formatContext, data_source, nullptr, &dictionary);
    // 释放字典
    av_dict_free(&dictionary);
    if (r) {
        // 把错误信息反馈给Java，回调给Java  Toast【打开媒体格式失败，请检查代码】
        if (jniCallbackHelper) {
            jniCallbackHelper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_OPEN_URL, av_err2str(r));
            // char * errorInfo = av_err2str(r); // 根据你的返回值 得到错误详情
        }
        avformat_close_input(&formatContext);
        return;
    }

    r = avformat_find_stream_info(formatContext, nullptr);
    if (r < 0) {
        if (jniCallbackHelper) {
            jniCallbackHelper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS, av_err2str(r));
        }
        avformat_close_input(&formatContext);
        return;
    }

    // AVCodecContext *codecContext = nullptr;

    for (int stream_index = 0; stream_index < formatContext->nb_streams; ++stream_index) {
        AVStream *stream = formatContext->streams[stream_index];

        AVCodecParameters *parameters = stream->codecpar;

        AVCodec *codec = const_cast<AVCodec *>(avcodec_find_decoder(parameters->codec_id));
        if (!codec) {
            if (jniCallbackHelper) {
                jniCallbackHelper->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL, av_err2str(r));
            }
            avformat_close_input(&formatContext);
        }

        codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            if (jniCallbackHelper) {
                jniCallbackHelper->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL, av_err2str(r));
            }

            avcodec_free_context(&codecContext); // 释放此上下文 avcodec 他会考虑到，你不用管*codec
            avformat_close_input(&formatContext);

            return;
        }

        r = avcodec_parameters_to_context(codecContext, parameters);
        if (r < 0) {
            if (jniCallbackHelper) {
                jniCallbackHelper->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL, av_err2str(r));
            }
            avcodec_free_context(&codecContext); // 释放此上下文 avcodec 他会考虑到，你不用管*codec
            avformat_close_input(&formatContext);
            return;
        }

        r = avcodec_open2(codecContext, codec, nullptr);
        if (r) { // 非0就是true
            if (jniCallbackHelper) {
                jniCallbackHelper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL, av_err2str(r));
            }
            avcodec_free_context(&codecContext); // 释放此上下文 avcodec 他会考虑到，你不用管*codec
            avformat_close_input(&formatContext);
            return;
        }

        if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) {

            // 虽然是视频类型，但是只有一帧封面
            if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                continue;
            }

        }
    } // for end

    if (jniCallbackHelper) {
        jniCallbackHelper->onPrepared(THREAD_CHILD);
    }

    LOGD("Prepare done");
}

// 1.把队列里面的压缩包（AVPacket *）取出来，然后解码成（AVFrame *）原始包 ----> 保存队列【真正干活的就是他】
void FFPlayer::video_decode() {
    AVPacket *pkt = nullptr;

    while (isStreaming) {

        if (frames.size() > 10) {
            av_usleep(10 * 1000); // 单位：microseconds 微妙 10毫秒
            continue;
        }

        if (packets.size() == 0) {
            av_usleep(10 * 1000);
            continue;
        }
        // LOGD("Get packet from queue");
        packets.getQueueAndDel(pkt);
        int ret = avcodec_send_packet(codecContext, pkt); // 第一步：把我们的 压缩包 AVPack发送给 FFmpeg缓存区
        if (ret) { // r != 0
            break; // avcodec_send_packet 出现了错误，结束循环
        }
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            // B帧  B帧参考前面成功  B帧参考后面失败   可能是P帧没有出来，再拿一次就行了
            LOGI("avcodec_receive_frame failed ret:%d", ret);
            av_frame_free(&frame); // 释放frame
            if (pkt != nullptr) {
                // 安心释放pkt本身空间释放 和 pkt成员指向的空间释放
                av_packet_unref(pkt); // 减1 = 0 释放成员指向的堆区
                releaseAVPacket(&pkt); // 释放AVPacket * 本身的堆区空间
            }

            // LOGD("packet size:%d", packets.size());
            continue;
        } else if (ret != 0) {
            if (frame) {
                releaseAVFrame(&frame);
            }
            if (pkt != nullptr) {
                // 安心释放pkt本身空间释放 和 pkt成员指向的空间释放
                av_packet_unref(pkt); // 减1 = 0 释放成员指向的堆区
                releaseAVPacket(&pkt); // 释放AVPacket * 本身的堆区空间
            }
            LOGE("ERROR :%d", ret);
            break; // 出错误了
        }

        // LOGD("Insert into que frames.size():%d", frames.size());
        if (frames.size() < 10) {
            frames.insertToQueue(frame);
        } else {
            av_frame_free(&frame);
        }

        if (pkt != nullptr) {
            // 安心释放pkt本身空间释放 和 pkt成员指向的空间释放
            av_packet_unref(pkt); // 减1 = 0 释放成员指向的堆区
            releaseAVPacket(&pkt); // 释放AVPacket * 本身的堆区空间
        }
    }

    // end while
    if (pkt != nullptr) {
        av_packet_unref(pkt); // 减1 = 0 释放成员指向的堆区
        releaseAVPacket(&pkt); // 释放AVPacket * 本身的堆区空间
    }

    LOGD("video decode end");
}

void FFPlayer::video_play() {
    AVFrame *frame = nullptr;
    uint8_t *dst_data[4];   // RGBA
    int dst_line_size[4];    // RGBA

    int ret = av_image_alloc(dst_data, dst_line_size,
                             codecContext->width, codecContext->height,
                             AV_PIX_FMT_RGBA, 1);
    if (ret < 0) {
        LOGE("av_image_alloc failed");
        return;
    }

    // yuv -> rgba
    SwsContext *sws_ctx = sws_getContext(
            // 下面是输入环节
            codecContext->width,
            codecContext->height,
            codecContext->pix_fmt, // 自动获取 xxx.mp4 的像素格式  AV_PIX_FMT_YUV420P // 写死的

            // 下面是输出环节
            codecContext->width,
            codecContext->height,
            AV_PIX_FMT_RGBA,
            SWS_BILINEAR, NULL, NULL, NULL);

    while (isStreaming) {

        ret = frames.getQueueAndDel(frame);
        if (!ret) { // ret == 0
            av_usleep(10 * 1000);
            continue; // 哪怕是没有成功，也要继续（假设：你生产太慢(原始包加入队列)，我消费就等一下你）
        }

        sws_scale(sws_ctx,
                // 下面是输入环节 YUV的数据
                  frame->data, frame->linesize,
                  0, codecContext->height,

                  // 下面是 输出环节 成果：RGBA数据 Android SurfaceView播放画面
                  dst_data,
                  dst_line_size
        );

        // LOGD("Trans to rgba finish");
        renderCallback(dst_data[0], codecContext->width, codecContext->height, dst_line_size[0]);

        if (frame) {
            av_frame_unref(frame); // 减1 = 0 释放成员指向的堆区
            releaseAVFrame(&frame); // 释放AVFrame * 本身的堆区空间
        }
    }
    // isPlaying = false;
    LOGD("video play end 1");
    av_freep(&dst_data[0]);
    LOGD("video play end 2");
    sws_freeContext(sws_ctx); // free(sws_ctx); FFmpeg必须使用人家的函数释放，直接崩溃
    LOGD("video play end");
}

void *task_video_decode(void *args) {
    auto *player = static_cast<FFPlayer *>(args);
    if (player != nullptr) {
        player->video_decode();
    }
    return nullptr;
}

void *task_video_play(void *args) {
    auto *player = static_cast<FFPlayer *>(args);
    if (player != nullptr) {
        player->video_play();
    }
    return nullptr;
}

void FFPlayer::start_decode_thread() {

    // 队列开始工作了
    packets.setWork(1); // 视频专属的packets队列
    frames.setWork(1); // 音频专属的frames队列

    // 第一个线程： 视频：取出队列的压缩包 进行解码 解码后的原始包 再push队列中去（视频：YUV）
    pthread_create(&pid_video_decode, nullptr, task_video_decode, this);

}

void FFPlayer::start_play_thread() {

    // 第二线线程：视频：从队列取出原始包，播放
    pthread_create(&pid_video_play, nullptr, task_video_play, this);

}


// 启动拉流的线程
void *start_stream_thread(void *args) {
    auto *player = static_cast<FFPlayer *>(args);
    if (player) {
        player->get_stream_process();
    } else {
        LOGE("task_start player is null");
    }
    return nullptr; // 必须返回，坑，错误很难找
}

void *task_stop(void *args) {
    auto *player = static_cast<FFPlayer *>(args);
    if (player != nullptr) {
        player->stop_streaming(player);
    }
    return nullptr;
}

void *task_prepare(void *args) { // 此函数和DerryPlayer这个对象没有关系，你没法拿DerryPlayer的私有成员
    auto *player = static_cast<FFPlayer *>(args);
    if (player == nullptr) {
        LOGE("player is null");
        return nullptr;
    }
    player->get_media_info();
    return nullptr; // 必须返回，坑，错误很难找
}

void FFPlayer::prepare() {
    pthread_create(&pid_prepare, nullptr, task_prepare, this); // this == DerryPlayer的实例
}

void FFPlayer::stop() {

    // 只要用户关闭了，就不准你回调给Java成 start播放
    if (jniCallbackHelper != nullptr) {
        DELETE(jniCallbackHelper)
    }

    // pthread_t pid_prepare = 0;
    // pthread_t pid_video_decode = 0;
    // pthread_t pid_start = 0;
    // pthread_t pid_video_play = 0;

    // 如果是直接释放 我们的 prepare_ start_ 线程，不能暴力释放 ，否则会有bug
    // 让他 稳稳的停下来
    // 我们要等这两个线程 稳稳的停下来后，我再释放DerryPlayer的所以工作
    // 由于我们要等 所以会ANR异常

    // 所以我们我们在开启一个 stop_线程 来等你 稳稳的停下来
    // 创建子线程
    pthread_create(&pid_stop, nullptr, task_stop, this);

}

void FFPlayer::start_streaming() {
    LOGD("start");
    isStreaming = true;

    // 分别启动两个线程用于解码跟播放
    start_decode_thread();
    start_play_thread();
    // 把 音频 视频 压缩包  加入队列里面去
    // 创建子线程 pthread
    pthread_create(&pid_start, nullptr, start_stream_thread, this); // this == DerryPlayer的实例

}
