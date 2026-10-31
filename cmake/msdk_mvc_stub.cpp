// Used only when Intel Media SDK's libmfx.lib is not available.
// H.264 MVC then falls back to the FFmpeg software decoder.
#include "decoders/ILAVDecoder.h"

ILAVDecoder *CreateDecoderMSDKMVC()
{
    return CreateDecoderAVCodec();
}
