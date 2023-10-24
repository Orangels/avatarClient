#ifndef PLAYER_H
#define PLAYER_H

#include <vector>
#include <portaudio.h>

class Player
{
    PaStream *pa_stream_;
    int sample_rate_;
    int channels_;
    int bytes_per_sample_;

public:
    Player();
    ~Player();

    bool Open(int id, int sample_rate, int channels, int bits_per_sample);
    void Close();
    int Write(unsigned char *data, int len);
};

#endif