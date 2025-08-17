extern "C"
{
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include "libavutil/imgutils.h"
#include "unistd.h"
#include <cstdlib>
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
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>

}
#include "sample_comm.h"
#include "rtsp_demo.h"
#include <vector>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>


using namespace std;
using namespace cv;
#define DISP_WIDTH  1920
#define DISP_HEIGHT 1080


int vi_dev_init() 
{
	printf("%s\n", __func__);
	int ret = 0;
	int devId = 0;	//设备ID
	int pipeId = devId;//管道ID

	VI_DEV_ATTR_S stDevAttr;//VI设备属性结构体
	VI_DEV_BIND_PIPE_S stBindPipe;//绑定管道的结构体

	memset(&stDevAttr, 0, sizeof(stDevAttr));
	memset(&stBindPipe, 0, sizeof(stBindPipe));
	// 0. 获取VI设备属性状态
	ret = RK_MPI_VI_GetDevAttr(devId, &stDevAttr);
	//如果VI设备没有配置
	if (ret == RK_ERR_VI_NOT_CONFIG) {
		// 0-1.配置VI设备属性
		ret = RK_MPI_VI_SetDevAttr(devId, &stDevAttr);
		if (ret != RK_SUCCESS) 
		{
			printf("RK_MPI_VI_SetDevAttr %x\n", ret);
			return -1;
		}
	} else {
		printf("RK_MPI_VI_SetDevAttr already\n");
	}
	// 1.获取VI设备使能状态
	ret = RK_MPI_VI_GetDevIsEnable(devId);
	if (ret != RK_SUCCESS) 
	{
		// 1-2.enable dev
		ret = RK_MPI_VI_EnableDev(devId);
		if (ret != RK_SUCCESS) {
			printf("RK_MPI_VI_EnableDev %x\n", ret);
			return -1;
		}
		// 1-3.绑定设备到管道
		stBindPipe.u32Num = 1;//绑定管道数量
		stBindPipe.PipeId[0] = pipeId;//管道ID
		ret = RK_MPI_VI_SetDevBindPipe(devId, &stBindPipe);
		if (ret != RK_SUCCESS) {
			printf("RK_MPI_VI_SetDevBindPipe %x\n", ret);
			return -1;
		}
	} else {
		printf("RK_MPI_VI_EnableDev already\n");
	}

	return 0;
}

//3：初始化 VI （视频输入）
int vi_init() 
{
    vi_dev_init();//初始化VI设备
    printf("初始化VI成功");

    //定义VI通道属性结构体
    VI_CHN_ATTR_S viattr={0};
    viattr.bFlip=RK_FALSE;//是否翻转
    viattr.bMirror=RK_FALSE;//镜像
    viattr.enAllocBufType=VI_ALLOC_BUF_TYPE_INTERNAL;
    viattr.enCompressMode=COMPRESS_MODE_NONE;//编码压缩模式（不压缩）
    viattr.enDynamicRange=DYNAMIC_RANGE_SDR10;//动态范围
    viattr.enPixelFormat = RK_FMT_YUV420SP;//像素格式（NV12）
    //viattr.stFrameRate={30,1};//帧率
	//设置ISP相关参数（负责对采集到的原始图像数据进行​​“美颜”和“优化”​​）
    viattr.stIspOpt.enCaptureType=VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE;//视频捕获模式
    viattr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;//内存类型：DMA加速（硬件加速）
    viattr.stIspOpt.stMaxSize = {1920,1080};//图像采集模块（ISP）所支持的最大分辨率
    viattr.stIspOpt.stWindow = {0,0,1920,1080};//图像采集窗口（坐标0,0）
    viattr.stIspOpt.u32BufCount = 3;//准备3个图像缓冲区，用于存放采集到的图像数据
    viattr.stIspOpt.u32BufSize = 1920*1080*2;//缓存大小
	//视频尺寸
    viattr.stSize.u32Height = 1080;
    viattr.stSize.u32Width = 1920;
    viattr.u32Depth = 3;
	//设置VI通道属性
    int ret = RK_MPI_VI_SetChnAttr(0, 0, &viattr);
	ret |= RK_MPI_VI_EnableChn(0, 0);//使能0号设备的0号通道
	if (ret) 
	{
		printf("ERROR: create VI error! ret=%d\n", ret);
		return ret;
	}
    return 0;
}

//初始化指定参数的VI通道
int vi_chn_init(int channelId, int width, int height) 
{
	int ret;
	int buf_cnt = 2;//设置图像缓冲区数量
	//VI通道属性结构体 VI_CHN_ATTR_S
	VI_CHN_ATTR_S vi_chn_attr;
	memset(&vi_chn_attr, 0, sizeof(vi_chn_attr));

	//设置 VI 缓冲区相关参数
	vi_chn_attr.stIspOpt.u32BufCount = buf_cnt;//2
	vi_chn_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF; // VI_V4L2_MEMORY_TYPE_MMAP;
	vi_chn_attr.stSize.u32Width = width;
	vi_chn_attr.stSize.u32Height = height;
	vi_chn_attr.enPixelFormat = RK_FMT_YUV420SP;//像素格式
	vi_chn_attr.enCompressMode = COMPRESS_MODE_NONE; // COMPRESS_AFBC_16x16;
	vi_chn_attr.u32Depth = 2; //深度，0, get fail, 1 - u32BufCount, can get, if bind to other device, must be < u32BufCount

	//设置并使能VI通道
	ret = RK_MPI_VI_SetChnAttr(0, channelId, &vi_chn_attr);
	ret |= RK_MPI_VI_EnableChn(0, channelId);
	if (ret) {
		printf("ERROR: create VI error! ret=%d\n", ret);
		return ret;
	}
	RK_MPI_VI_StartPipe(0);
	return ret;
}

//初始化视频编码格式（VENC）- VI摄像头采集视频压缩编码成视频码流
//负责将​原始视频数据压缩编码成特定格式的视频流​​，比如 H.264、H.265 (HEVC)、MJPEG等
int venc_init(int chnId, int width, int height, RK_CODEC_ID_E enType) 
{
	printf("%s\n",__func__);

	//定义编码器接受参数和属性结构体
	VENC_RECV_PIC_PARAM_S stRecvParam;
	VENC_CHN_ATTR_S stAttr;
	memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));

	//视频编码类型：H.264编码
	if (enType == RK_VIDEO_ID_AVC) 
	{
		//stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
		//stAttr.stRcAttr.stH264Cbr.u32BitRate = 5* 1024;//5M
		//stAttr.stRcAttr.stH264Cbr.u32Gop = 60;
		stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264VBR;
		stAttr.stRcAttr.stH264Vbr.u32Gop = 30;//关键帧间隔为30帧
		stAttr.stRcAttr.stH264Vbr.u32BitRate = 5* 1024;
		stAttr.stRcAttr.stH264Vbr.u32MaxBitRate  = 8* 1024;
		stAttr.stRcAttr.stH264Vbr.u32MinBitRate = 2*1024;
		
	} 
	//视频编码类型：H.265编码
	else if (enType == RK_VIDEO_ID_HEVC) 
	{
		stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
		stAttr.stRcAttr.stH265Cbr.u32BitRate = 10 * 1024;
		stAttr.stRcAttr.stH265Cbr.u32Gop = 60;//关键帧间隔为60帧
	} 
	//视频编码类型：MJPEG编码
	else if (enType == RK_VIDEO_ID_MJPEG) 
	{
		stAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
		stAttr.stRcAttr.stMjpegCbr.u32BitRate = 10 * 1024;
	}
	//设置上面三种视频编码类型通用编码参数
	stAttr.stVencAttr.enType = enType;
	stAttr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;//像素格式
	if (enType == RK_VIDEO_ID_AVC)
		stAttr.stVencAttr.u32Profile = H264E_PROFILE_HIGH;//H.264高档次
	stAttr.stVencAttr.u32PicWidth = width;
	stAttr.stVencAttr.u32PicHeight = height;
	stAttr.stVencAttr.u32VirWidth = width;
	stAttr.stVencAttr.u32VirHeight = height;
	stAttr.stVencAttr.u32StreamBufCnt = 2;
	stAttr.stVencAttr.u32BufSize = width * height * 3 / 2;
	stAttr.stVencAttr.enMirror = MIRROR_NONE;

	//创建一个指定ID的视频编码通道​​，并应用刚才配置的参数
	RK_MPI_VENC_CreateChn(chnId, &stAttr);
	//配置接收参数并启动接受帧
	memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
	stRecvParam.s32RecvPicNum = -1;
	RK_MPI_VENC_StartRecvFrame(chnId, &stRecvParam);
	
	return 0;
}

//初始化ISP
void Init_ISP(void)
{
    RK_BOOL multi_sensor = RK_FALSE;//不使用多传感器	
	const char *iq_dir = "/etc/iqfiles";//图像质量参数文件路径
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
	//hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
	SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
	//运行ISP
	SAMPLE_COMM_ISP_Run(0);
}

//初始化VENC（视频编码器）
void Init_VenC(void)
{  
    //配置编码类型为H.264
	RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC;
	venc_init(0, 1920, 1080, enCodecType);
    //配置VENC通道与VI通道的绑定关系
    MPP_CHN_S src;
    MPP_CHN_S dst;
    src.enModId = RK_ID_VI;
    src.s32ChnId = 0;
    src.s32DevId = 0;

    dst.enModId = RK_ID_VENC;
    dst.s32ChnId = 0;
    dst.s32DevId = 0;
    RK_MPI_SYS_Bind(&src,&dst);//绑定VI到VENC通道--把​VI通道0​​绑定到VENC通道0​​									
}


//主函数
int main(int argc,char * argv[])
{
	//停止系统中可能冲突的服务
    system("RkLunch-stop.sh");
    
	//1：初始化 ISP
    RK_BOOL multi_sensor = RK_FALSE;	
	const char *iq_dir = "/etc/iqfiles";
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
	//hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
	SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
	SAMPLE_COMM_ISP_Run(0);
    printf("初始化 ISP 成功！\r\n");

	//2：初始化 RKMPI（瑞芯维媒体处理接口）
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
		RK_LOGE("rk mpi sys init fail!");
		return -1;
	}
    printf("初始化 MPI 系统成功！\r\n");

    vi_dev_init();
    printf("初始化VI成功");

	//3：初始化 VI （视频输入通道）
    //FRAME_RATE_CTRL_S   stFrameRate = {}; 
    VI_CHN_ATTR_S viattr={0};
    viattr.bFlip=RK_FALSE;//是否翻转
    viattr.bMirror=RK_FALSE;//镜像
    viattr.enAllocBufType=VI_ALLOC_BUF_TYPE_INTERNAL;
    viattr.enCompressMode=COMPRESS_MODE_NONE;//编码压缩模式
    viattr.enDynamicRange=DYNAMIC_RANGE_SDR10;//动态范围
    viattr.enPixelFormat = RK_FMT_YUV420SP;//YUV420SP格式
    //viattr.stFrameRate={30,1};//帧率

	//ISP参数配置
    viattr.stIspOpt.enCaptureType=VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE;
    viattr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;//DMA加速
    viattr.stIspOpt.stMaxSize = {1920,1080};
    viattr.stIspOpt.stWindow = {0,0,1920,1080};
    viattr.stIspOpt.u32BufCount = 3;//3个缓存
    viattr.stIspOpt.u32BufSize = 1920*1080*2;
    viattr.stSize.u32Height = 1080;
    viattr.stSize.u32Width = 1920;
    viattr.u32Depth = 3;

	//设置VI通道属性并使能
    int ret = RK_MPI_VI_SetChnAttr(0, 0, &viattr);
	ret |= RK_MPI_VI_EnableChn(0, 0);
	if (ret) {
		printf("ERROR: create VI error! ret=%d\n", ret);
		return ret;
	}
    
    Init_VenC();//初始化VENC（编码器）

    VIDEO_FRAME_INFO_S stViFrame;//VI原始帧
    VENC_STREAM_S stFrame;	//VENC编码后的流
	//为编码流分配内存
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
    int fps_count = 0;
 
	//持续采集和编码视频
    while(1)
    {
        // 从VI获取一帧图像
        RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
		// 将原始帧发送到编码器
        RK_MPI_VENC_SendFrame(0,  &stViFrame ,-1);
		// 从编码器获取编码后的流
        ret  = RK_MPI_VENC_GetStream(0, &stFrame, -1);
		if(ret == RK_SUCCESS)
        {   
            printf("获取视频数据且编码成功 当前帧数据长度==%d 当前帧时间戳==%lld\r\n",stFrame.pstPack->u32Len,stFrame.pstPack->u64PTS);
            //(uint8_t *)RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
			 // 释放VI帧缓存
            RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
			// 释放VENC流缓存
            RK_MPI_VENC_ReleaseStream(0, &stFrame);
        }
    }
    return 0;
}
