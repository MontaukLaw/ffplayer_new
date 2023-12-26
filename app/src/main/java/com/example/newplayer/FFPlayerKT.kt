package com.example.newplayer

import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent


@Suppress("DEPRECATION")
class FFPlayerKT : SurfaceHolder.Callback, LifecycleObserver {

    val TAG = "FFPlayerKT"

    private var nativePlayerObj: Long? = null // 保存DerryPlayer.cpp对象的地址
    private var onPreparedListener: OnPreparedListener? = null // C++层准备情况的接口
    private var onErrorListener: OnErrorListener? = null
    private var surfaceHolder: SurfaceHolder? = null
    private var dataSource: String? = null

    override fun surfaceCreated(p0: SurfaceHolder) {
    }

    override fun surfaceChanged(p0: SurfaceHolder, p1: Int, p2: Int, p3: Int) {
    }

    override fun surfaceDestroyed(p0: SurfaceHolder) {
    }

    fun setOnErrorListener(onErrorListener: OnErrorListener?) {
        this.onErrorListener = onErrorListener
    }

    fun onPrepared() {
        if (onPreparedListener != null) {
            Log.d(TAG, "onPrepared: ")
            onPreparedListener!!.onPrepared()
        }
    }

    fun setOnPreparedListener(onPreparedListener: OnPreparedListener?) {
        this.onPreparedListener = onPreparedListener
    }

    fun setSurfaceView(surfaceView: SurfaceView) {
        if (surfaceHolder != null) {
            surfaceHolder?.removeCallback(this) // 清除上一次的
        }
        surfaceHolder = surfaceView.holder
        surfaceHolder?.addCallback(this) // 监听
    }

    fun onError(errorCode: Int, ffmpegError: String) {
        val title = "\nFFmpeg给出的错误如下:\n"
        if (null != onErrorListener) {
            var msg: String? = null
            when (errorCode) {
                FFMPEG.FFMPEG_CAN_NOT_OPEN_URL -> msg = "打不开视频$title$ffmpegError"
                FFMPEG.FFMPEG_CAN_NOT_FIND_STREAMS -> msg = "找不到流媒体$title$ffmpegError"
                FFMPEG.FFMPEG_FIND_DECODER_FAIL -> msg = "找不到解码器$title$ffmpegError"
                FFMPEG.FFMPEG_ALLOC_CODEC_CONTEXT_FAIL -> msg = "无法根据解码器创建上下文$title$ffmpegError"
                FFMPEG.FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL -> msg = "根据流信息 配置上下文参数失败$title$ffmpegError"
                FFMPEG.FFMPEG_OPEN_DECODER_FAIL -> msg = "打开解码器失败$title$ffmpegError"
                FFMPEG.FFMPEG_NOMEDIA -> msg = "没有音视频$title$ffmpegError"
            }
            onErrorListener!!.onError(msg)
        }
    }

    fun start() {
        Log.d(TAG, "Start: ")
        startNative(nativePlayerObj!!)
    }

    fun setDataSource(dataSource: String?) {
        this.dataSource = dataSource
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
    fun release() {
        Log.d(TAG, "Release: ")
        releaseNative(nativePlayerObj!!)
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun stop() {
        Log.d(TAG, "Stop: ")
        stopNative(nativePlayerObj!!)
        nativePlayerObj = null
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    fun prepare() {
        // 当前Activity处于Resumed状态时调用
        Log.d(TAG, "Getting media info: ")
        nativePlayerObj = prepareNative(dataSource!!)
    }

    private external fun setSurfaceNative(surface: Surface, nativeObj: Long)
    private external fun startNative(nativeObj: Long)
    private external fun stopNative(nativeObj: Long)
    private external fun prepareNative(dataSource: String): Long
    private external fun releaseNative(nativeObj: Long)
}