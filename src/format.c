#include "format.h"

void format_from_av(AVCodecContext *src, format_t *dst)
{
  dst->samplerate = src->sample_rate;
  dst->bitspersample =  av_get_bytes_per_sample(src->sample_fmt) * 8;
  dst->channels = src->channels;
  dst->extradata = src->extradata;
  dst->extradata_size = src->extradata_size;
  dst->frame_size =
    src->frame_size * src->channels * av_get_bytes_per_sample(src->sample_fmt);
}