#include<AL/al.h>
#include<AL/alc.h>
#include<mpg123.h>
#include<stdexcept>
#include<thread>
#include<array>
#include<stdio.h>
#include<termios.h>
#include<unistd.h>
#include"KCipher2.h"

int main(int argc, char** argv)
{
    if (argc != 2) {
        throw std::runtime_error("se esperaba un argumento");
    }
    /*************************************************************************/
    if (mpg123_init() != MPG123_OK) {
        throw std::runtime_error("mpg123_init fallo");
    }

    mpg123_handle* mpegHandle = mpg123_new(NULL, NULL);
    if (!mpegHandle) {
        throw std::runtime_error("mpg123_new retorno null");
    }

    if (mpg123_open(mpegHandle, argv[1]) != MPG123_OK) {
        throw std::runtime_error("mpg123_open fallo");
    }

    long rate;
    int encoding;
    int nchannels;
    if (mpg123_getformat(mpegHandle, &rate, &nchannels, &encoding) != MPG123_OK) {
        throw std::runtime_error("mpg123_getformat fallo");
    }

    if(mpg123_format_none(mpegHandle) != MPG123_OK) {
        throw std::runtime_error("mpg123_format_none fallo");
    }

    if (mpg123_format(mpegHandle, rate, nchannels, encoding) != MPG123_OK) {
        throw std::runtime_error("mpg123_format fallo");
    }
    /*************************************************************************/
    ALCdevice* alcDevice = alcOpenDevice(NULL);
    if (!alcDevice) {
        throw std::runtime_error("alcOpenDevice retorno null");
    }

    ALCcontext* alcContext = alcCreateContext(alcDevice, NULL);
    if (!alcContext) {
        throw std::runtime_error("alcCreateContext fallo");
    }

    if (!alcMakeContextCurrent(alcContext)) {
        throw std::runtime_error("alcMakeContextCurrent fallo");
    }
    /*************************************************************************/
    ALuint alSource;
    alGenSources(1, &alSource);
    std::array<unsigned char, 16384> mpegBuffer;

    ALuint alBuffers[8];
    alGenBuffers(8, alBuffers);
    for (int i = 0; i < 8; i++) {
        size_t outSize;
        mpg123_read(mpegHandle, mpegBuffer.data(), mpegBuffer.size(), &outSize);
        alBufferData(alBuffers[i], nchannels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, mpegBuffer.data(), outSize, rate);
        alSourceQueueBuffers(alSource, 1, &alBuffers[i]);
    }
    alSourcePlay(alSource);
    /*************************************************************************/
    uint32_t key[4] = { 00000000,00000000, 00000000, 00000000 };
    uint32_t iv[4] = { 00000000, 00000000, 00000000, 00000000 };
    init(key, iv);
    /*************************************************************************/
    bool exit = false;
    bool cifrar = false;
    std::thread thread = std::thread([&]() {
        do {
            ALint processed = 0;
            alGetSourcei(alSource, AL_BUFFERS_PROCESSED, &processed);

            if (!processed) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                do {
                    ALuint buffer;
                    alSourceUnqueueBuffers(alSource, 1, &buffer);

                    size_t outSize;
                    int code = mpg123_read(mpegHandle, mpegBuffer.data(), mpegBuffer.size(), &outSize);
                    if (code != MPG123_OK) {
                        if (code == MPG123_DONE) {
                            exit = true;
                            break;
                        } else {
                            throw std::runtime_error("mpg123_read fallo");
                        }
                    }
                    if (cifrar) {
                        operar(mpegBuffer.data(), outSize);
                    }

                    alBufferData(buffer, nchannels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, mpegBuffer.data(), outSize, rate);
                    alSourceQueueBuffers(alSource, 1, &buffer);
                } while (--processed);

                ALint val;
                alGetSourcei(alSource, AL_SOURCE_STATE, &val);
                if(val != AL_PLAYING) {
                    alSourcePlay(alSource);
                }
            }
        } while(!exit);
    });
    /*************************************************************************/
    int c;
    do {
        termios oldt;
        termios newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        c = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        cifrar = !cifrar;
    } while (c != 'q');
    exit = true;
    /*************************************************************************/
    if (thread.joinable()) {
        thread.join();
    }
    mpg123_delete(mpegHandle);
    mpg123_exit();
    alDeleteSources(1, &alSource);
    alDeleteBuffers(8, alBuffers);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(alcContext);
    alcCloseDevice(alcDevice);
    return 0;
}
