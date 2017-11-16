#include"OGLWindow.h"
#include"OGLUtils.h"
#include"KCipher2.h"
#include<stdexcept>
#include<array>

extern "C" {
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
#include<libavutil/error.h>
#include<libavutil/opt.h>
#include<libswscale/swscale.h>
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        throw std::runtime_error("se esperaba un argumento");
    }
    /************************************************************************************************/
    int code;
    av_register_all();
    AVFormatContext* avFormatContext = NULL;

    code = avformat_open_input(&avFormatContext, argv[1], NULL, NULL);
    if (code < 0) {
        std::array<char, 1024> error;
        av_strerror(code, error.data(), 1024);
        throw std::runtime_error("avformat_open_input: " + std::string(error.begin(), error.end()));
    }

    code = avformat_find_stream_info(avFormatContext, NULL);
    if (code < 0) {
        std::array<char, 1024> error;
        av_strerror(code, error.data(), 1024);
        throw std::runtime_error("avformat_find_stream_info: " + std::string(error.begin(), error.end()));
    }

    int videoStreamIdx = av_find_best_stream(avFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (videoStreamIdx < 0) {
        throw std::runtime_error("av_find_best_stream fallo");
    }
    /************************************************************************************************/
    AVStream* videoStream = avFormatContext->streams[videoStreamIdx];
    AVCodecContext* avCC = videoStream->codec;

    AVCodec* dec = avcodec_find_decoder(avCC->codec_id);
    if (!dec) {
        throw std::runtime_error("avcodec_find_decoder fallo");
    }

    if ((avcodec_open2(avCC, dec, NULL)) < 0) {
        throw std::runtime_error("avcodec_open2 fallo");
    }
    /************************************************************************************************/
    AVFrame* avFrame = avcodec_alloc_frame();
    if (!avFrame) {
        throw std::runtime_error("avcodec_alloc_frame fallo");
    }

    AVFrame* avRGBFrame = avcodec_alloc_frame();
    if (!avRGBFrame) {
        throw std::runtime_error("avcodec_alloc_frame fallo");
    }

    int numBytes = avpicture_get_size(PIX_FMT_BGRA, avCC->width, avCC->height);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes*sizeof(uint8_t));
    avpicture_fill((AVPicture*)avRGBFrame, buffer, PIX_FMT_BGRA, avCC->width, avCC->height);
    /************************************************************************************************/
    SwsContext* swsContext = sws_alloc_context();

    av_opt_set_int(swsContext, "srcw", avCC->width, 0);
    av_opt_set_int(swsContext, "srch", avCC->height, 0);
    av_opt_set_int(swsContext, "dstw", avCC->width, 0);
    av_opt_set_int(swsContext, "dsth", avCC->height, 0);
    av_opt_set_int(swsContext, "src_format", avCC->pix_fmt, 0);
    av_opt_set_int(swsContext, "dst_format", PIX_FMT_BGRA, 0);
    av_opt_set_int(swsContext, "sws_flags", SWS_FAST_BILINEAR, 0);
    av_opt_set_int(swsContext, "src_range", 1, 0);
    av_opt_set_int(swsContext, "dst_range", 1, 0);

    int table[4] = {104597, 132201, 25675, 53279};
    if (sws_setColorspaceDetails(swsContext, table, 1, table, 1, 0, 1<<16, 1<<16) < 0) {
        throw std::runtime_error("sws_setColorspaceDetails fallo");
    }

    if (sws_init_context(swsContext, NULL, NULL) < 0) {
        throw std::runtime_error("sws_init_context fallo");
    }
    /************************************************************************************************/
    OGLWindowDescription desc;
    desc.title = argv[1];
    desc.width = avCC->width;
    desc.height = avCC->height;
    desc.x = 0.5f*(utils::getDesktopWidth() - desc.width);
    desc.y = 0.5f*(utils::getDesktopHeight() - desc.height);

    OGLWindow oglwindow(desc);
    /**********************************************************************************/
    OGLXWindowHandle glxWindow;
    oglwindow.getNativeGLXWindow(glxWindow);
    OGLDisplayHandle display;
    oglwindow.getNativeDisplay(display);
    OGLFBConfig fbc;
    oglwindow.getNativeFBConfig(fbc);
    GLXContext context = NULL;
    context = glXCreateNewContext(display, fbc, GLX_RGBA_TYPE, 0, True);
    glXMakeContextCurrent(display, glxWindow, glxWindow, context);
    /**********************************************************************************/
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, avCC->width, avCC->height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    /**********************************************************************************/
    uint32_t key[4] = { 00000000,00000000, 00000000, 00000000 };
    uint32_t iv[4] = { 00000000, 00000000, 00000000, 00000000 };
    init(key, iv);
    /**********************************************************************************/

    OGLWindowEvent event;
    int width = 0;
    int height = 0;
    bool cifrar = false;
    oglwindow.show();

    for(;;) {
        bool exit = false;
        while (oglwindow.pendingEvent()) {
            oglwindow.getEvent(event);

            switch(event.type) {
            case OGL_WINDOW_SIZE:
                glViewport(0, 0, OGL_WINDOW_SIZE_GET_WIDTH(event), OGL_WINDOW_SIZE_GET_HEIGHT(event));
                width = OGL_WINDOW_SIZE_GET_WIDTH(event);
                height = OGL_WINDOW_SIZE_GET_HEIGHT(event);
                break;

            case OGL_WINDOW_KEY_DOWN:
                if (OGL_WINDOW_KEY_GET_CODE(event) == 41) {
                    int state = oglwindow.getState();
                    if (state == OGL_STATE_NONE) {
                        break;
                    }
                    if (!(state & OGL_STATE_FULLSCREEN)) {
                        oglwindow.fullscreen();
                    } else {
                        oglwindow.restore();
                    }
                } else {
                    cifrar = !cifrar;
                }
                break;

            case OGL_WINDOW_CLOSE:
                exit = true;
                break;
            }
        }

        if (exit) {
            break;
        }

        AVPacket packet;
        while(av_read_frame(avFormatContext, &packet) >= 0) {

            bool exit = false;
            int frameFinished;
            if(packet.stream_index == videoStreamIdx) {
                avcodec_decode_video2(avCC, avFrame, &frameFinished, &packet);
                if(frameFinished) {
                    sws_scale(swsContext, avFrame->data, avFrame->linesize, 0, avCC->height, avRGBFrame->data, avRGBFrame->linesize);
                    /**********************************************************************************/
                    if (cifrar) {
                        operar(avRGBFrame->data[0], numBytes*sizeof(uint8_t));
                    }

                    /**********************************************************************************/
                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, texture);
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, avCC->width, avCC->height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, avRGBFrame->data[0]);

                    glBegin(GL_QUADS);

                    if (avCC->width/(float)avCC->height > width/(float)height) {
                        int newHeight = (width*avCC->height)/avCC->width;
                        int topY = (height - newHeight)/2.0f;
                        int buttomY = topY + newHeight;
                        /*abajo-izq*/
                        glTexCoord2f(0.0f, 0.0f);
                        glVertex2f(-1.0f, -1.0f + 2.0f*(buttomY/(float)height));
                        /*abajo-der*/
                        glTexCoord2f(1.0f, 0.0f);
                        glVertex2f(1.0f, -1.0f + 2.0f*(buttomY/(float)height));
                        /*arriba-der*/
                        glTexCoord2f(1.0f, 1.0f);
                        glVertex2f(1.0f, -1.0f + 2.0f*(topY/(float)height));
                        /*arriba-izq*/
                        glTexCoord2f(0.0f, 1.0f);
                        glVertex2f(-1.0f, -1.0f + 2.0f*(topY/(float)height));

                    } else {
                        int newWidth = (height*avCC->width)/avCC->height;
                        int xLeft = (width - newWidth)/2.0f;
                        int xRight = xLeft + newWidth;
                        /*abajo-izq*/
                        glTexCoord2f(0.0f, 0.0f);
                        glVertex2f(-1.0f + 2.0f*(xLeft/(float)width), 1.0f);
                        /*abajo-der*/
                        glTexCoord2f(1.0f, 0.0f);
                        glVertex2f(-1.0f + 2.0f*(xRight/(float)width), 1.0f);
                        /*arriva-der*/
                        glTexCoord2f(1.0f, 1.0f);
                        glVertex2f(-1.0f + 2.0f*(xRight/(float)width), -1.0f);
                        /*arriba-izq*/
                        glTexCoord2f(0.0f, 1.0f);
                        glVertex2f(-1.0f + 2.0f*(xLeft/(float)width), -1.0f);
                    }
                    glEnd();
                    /**********************************************************************************/
                    exit = true;
                }
            }
            av_free_packet(&packet);
            if (exit) {
                break;
            }
        }

        glXSwapBuffers(display, glxWindow);
    }

    glXMakeCurrent(display, 0, 0);
    glXDestroyContext(display, context);
    glDeleteTextures(1, &texture);
    sws_freeContext(swsContext);
    av_free(buffer);
    av_free(avFrame);
    av_free(avRGBFrame);
    avcodec_close(avCC);
    avformat_close_input(&avFormatContext);
    return 0;
}
