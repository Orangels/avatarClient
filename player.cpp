#include "player.h"
#include "Misc.h"
#include "StringUtil.h"

Player::Player()
{
    sample_rate_ = 0;
    channels_ = 0;
    bytes_per_sample_ = 0;
    pa_stream_ = nullptr;
}

Player::~Player()
{
    Close();
}

bool Player::Open(int id, int sample_rate, int channels, int bits_per_sample)
{
    sample_rate_ = sample_rate;
    channels_ = channels;
    bytes_per_sample_ = bits_per_sample / 8;

    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(id);

    PaSampleFormat sampleFormat = 0;
    if (bits_per_sample == 8)
        sampleFormat = paInt8;
    else if (bits_per_sample == 16)
        sampleFormat = paInt16;
    else if (bits_per_sample == 32)
        sampleFormat = paInt32;
    else
    {
        logger::Log(L"Unsupported BitsPerSample: %d\n", bits_per_sample);
        return false;
    }

    PaStreamParameters outputParameters = {0};
    outputParameters.device = id;
    outputParameters.channelCount = channels;
    outputParameters.sampleFormat = sampleFormat;
    outputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;

    PaError err = Pa_OpenStream(&pa_stream_, nullptr, &outputParameters, sample_rate, paFramesPerBufferUnspecified, paNoFlag, nullptr, nullptr);
    if (err)
    {
        logger::Log(L"Can not open the audio output stream! %s\n", util::Utf8ToUnicode(Pa_GetErrorText(err)).c_str());
        return false;
    }

    err = Pa_StartStream(pa_stream_);
    if (err)
    {
        logger::Log(L"Fail to start audio output stream! %s\n", util::Utf8ToUnicode(Pa_GetErrorText(err)).c_str());
        return false;
    }

    return true;
}

void Player::Close()
{
    if (pa_stream_)
    {
        Pa_StopStream(pa_stream_);
        Pa_CloseStream(pa_stream_);
        pa_stream_ = nullptr;
    }
}

int Player::Write(unsigned char* data, int len)
{
    PaError err = paNoError;
    if (pa_stream_)
    {
        err = Pa_WriteStream(pa_stream_, data, len / bytes_per_sample_);
    }
    return err;
}
