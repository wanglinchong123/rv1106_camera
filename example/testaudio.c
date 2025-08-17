
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include "libavutil/imgutils.h"
#include "unistd.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavfilter/avfilter.h"



int main(int argc, char **argv)
{
    avdevice_register_all();
    //1:打开设备
    AVDictionary *options = NULL;
    av_dict_set(&options, "sample_rate", "48000", 0);
    av_dict_set(&options, "channels", "2", 0);
    av_dict_set(&options, "format", "s16", 0); // ALSA的S16LE格式
    AVInputFormat * alsa_device = av_find_input_format("alsa");
    if(alsa_device == NULL)
    {
        printf("没有声卡设备库!");
        return -1;
    }
    //AVDictionary * ops;


    AVFormatContext *ifmt_ctx = NULL;//创建上下文 
    int ret = avformat_open_input(&ifmt_ctx, "hw:0", alsa_device, &options);
    if(ret !=0)
    {
        printf("打开声卡设备失败！\r\n");
        return -1;
    }

    printf("成功打开声卡设备！\r\n");

    //2:创建文件
    char * outfile = "audio_out.aac";
    AVOutputFormat * fmt = av_guess_format(NULL,outfile,NULL);
    if(fmt ==NULL)
    {
        printf("未寻找到合适的格式！\r\n");
        return -1;
    }
    printf("寻找到了合适的AAC格式！\r\n");

    //3:创建编码器
    printf("fmt->audio_codec==%d\r\n",fmt->audio_codec);
    //AVCodec * audio_codec = avcodec_find_encoder(fmt->audio_codec);
    AVCodec * audio_codec = avcodec_find_encoder_by_name("libfdk_aac");
    if(audio_codec == NULL)
    {
        printf("FFMPEG不支持该编码器!\r\n");
        return -1;
    }
    AVCodecContext * audio_codectext = avcodec_alloc_context3(audio_codec);
    if(audio_codectext == NULL)
    {
        printf("编码器上下文创建失败！\r\n");
        return -1;
    }
    audio_codectext->sample_fmt = AV_SAMPLE_FMT_S16;
    audio_codectext->sample_rate = 48000;
    audio_codectext->channels = 2;
    audio_codectext->channel_layout = AV_CH_LAYOUT_STEREO;
    audio_codectext->bit_rate = 64000;
    audio_codectext->codec_id = fmt->audio_codec;
    audio_codectext->frame_size= 1024;
    ret = avcodec_open2(audio_codectext, audio_codec, NULL);//绑定编码器和上下文
    if(ret !=0)
    {
        printf("编码器绑定失败！\r\n");
        return -1;
    }

/*
    AVFrame * pFrame = av_frame_alloc();
	pFrame->nb_samples= audio_codectext->frame_size;
	pFrame->format= audio_codectext->sample_fmt;

    int audio_size = av_samples_get_buffer_size(NULL, audio_codectext->channels,audio_codectext->frame_size,audio_codectext->sample_fmt, 1);
	uint8_t* frame_buf = (uint8_t *)av_malloc(audio_size);
	avcodec_fill_audio_frame(pFrame, audio_codectext->channels, audio_codectext->sample_fmt,(const uint8_t*)frame_buf, audio_size, 1);
 */   

    AVFrame *frame = av_frame_alloc();
    
    frame->format         = audio_codectext->sample_fmt;
    frame->nb_samples     = audio_codectext->frame_size;
    frame->channel_layout = audio_codectext->channel_layout;
    av_frame_get_buffer(frame, 0);
    printf("framelinesize==%d\r\n",frame->linesize[0]);
    //avformat_write_header(audio_codectext,NULL);//写音频头


    FILE * file = fopen("test.aac","wb");
    FILE * file2 = fopen("test.pcm","wb");
    AVPacket pkt;
    AVPacket * readpkt = av_packet_alloc();
    
    int count = 0;
    while(av_read_frame(ifmt_ctx,&pkt)==0)
    {
        fwrite(pkt.data,1,pkt.size,file2);
        memcpy(frame->data[0]+count*pkt.size,pkt.data,pkt.size);
        count++;
        //printf("count == %d\t\t pkt.size==%d\r\n",count,pkt.size);
        //printf("pkt.size*count == %d\r\n",pkt.size*count);
       
        if(pkt.size*count >= 1024)
        {   
            count=0;
            frame->linesize[0] = 1024;
            frame->nb_samples = 1024/4;//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            avcodec_send_frame(audio_codectext,frame);
           
            while(1)
            {
                ret = avcodec_receive_packet(audio_codectext,readpkt);
                if(ret == 0)
                {
                    //printf("编码成功！\r\n!\r\n");
                    av_packet_rescale_ts(&readpkt, 
                    (AVRational){1, audio_codectext->sample_rate}, 
                    ifmt_ctx->streams[0]->time_base);
                    fwrite(readpkt->data,1,readpkt->size,file);
                    fflush(NULL);
                    av_packet_unref(readpkt);
                }else if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                break;
            }
            
            av_packet_unref(&pkt);
        }else
        {
            av_packet_unref(&pkt);
            continue;
        }
    }
    return 0;
}