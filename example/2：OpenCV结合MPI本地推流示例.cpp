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
RK_U64 TEST_COMM_GetNowUs() 
{
	struct timespec time = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
}
int vi_dev_init() 
{
	printf("%s\n", __func__);
	int ret = 0;
	int devId = 0;
	int pipeId = devId;

	VI_DEV_ATTR_S stDevAttr;
	VI_DEV_BIND_PIPE_S stBindPipe;
	
	memset(&stDevAttr, 0, sizeof(stDevAttr));
	memset(&stBindPipe, 0, sizeof(stBindPipe));
	// 0. get dev config status
	ret = RK_MPI_VI_GetDevAttr(devId, &stDevAttr);
	if (ret == RK_ERR_VI_NOT_CONFIG) 
	{
		// 0-1.config dev
		ret = RK_MPI_VI_SetDevAttr(devId, &stDevAttr);
		if (ret != RK_SUCCESS) {
			printf("RK_MPI_VI_SetDevAttr %x\n", ret);
			return -1;
		}
	} else {
		printf("RK_MPI_VI_SetDevAttr already\n");
	}
	// 1.get dev enable status
	ret = RK_MPI_VI_GetDevIsEnable(devId);
	if (ret != RK_SUCCESS) 
	{
		// 1-2.enable dev
		ret = RK_MPI_VI_EnableDev(devId);
		if (ret != RK_SUCCESS) {
			printf("RK_MPI_VI_EnableDev %x\n", ret);
			return -1;
		}
		// 1-3.bind dev/pipe
		stBindPipe.u32Num = 1;
		stBindPipe.PipeId[0] = pipeId;
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
int vi_init() 
{
    vi_dev_init();
    printf("初始化VI成功");
//3：初始化 VI 
    //FRAME_RATE_CTRL_S   stFrameRate = {}; 
    VI_CHN_ATTR_S viattr={0};
    viattr.bFlip=RK_FALSE;//是否翻转
    viattr.bMirror=RK_FALSE;//镜像
    viattr.enAllocBufType=VI_ALLOC_BUF_TYPE_INTERNAL;
    viattr.enCompressMode=COMPRESS_MODE_NONE;//编码压缩模式
    viattr.enDynamicRange=DYNAMIC_RANGE_SDR10;//动态范围
    viattr.enPixelFormat = RK_FMT_YUV420SP;//NV12
    //viattr.stFrameRate={30,1};//帧率
    viattr.stIspOpt.enCaptureType=VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE;
    viattr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;//DMA加速
    viattr.stIspOpt.stMaxSize = {1920,1080};
    viattr.stIspOpt.stWindow = {0,0,1920,1080};
    viattr.stIspOpt.u32BufCount = 3;
    viattr.stIspOpt.u32BufSize = 1920*1080*2;
    viattr.stSize.u32Height = 1080;
    viattr.stSize.u32Width = 1920;
    viattr.u32Depth = 3;
    int ret = RK_MPI_VI_SetChnAttr(0, 0, &viattr);
	ret |= RK_MPI_VI_EnableChn(0, 0);
	if (ret) {
		printf("ERROR: create VI error! ret=%d\n", ret);
		return ret;
	}
    return 0;
}

int vi_chn_init(int channelId, int width, int height) {
	int ret;
	int buf_cnt = 2;
	// VI init
	VI_CHN_ATTR_S vi_chn_attr;
	memset(&vi_chn_attr, 0, sizeof(vi_chn_attr));
	vi_chn_attr.stIspOpt.u32BufCount = buf_cnt;
	vi_chn_attr.stIspOpt.enMemoryType =
	VI_V4L2_MEMORY_TYPE_DMABUF; // VI_V4L2_MEMORY_TYPE_MMAP;
	vi_chn_attr.stSize.u32Width = width;
	vi_chn_attr.stSize.u32Height = height;
	vi_chn_attr.enPixelFormat = RK_FMT_YUV420SP;
	vi_chn_attr.enCompressMode = COMPRESS_MODE_NONE; // COMPRESS_AFBC_16x16;
	vi_chn_attr.u32Depth = 2; //0, get fail, 1 - u32BufCount, can get, if bind to other device, must be < u32BufCount
	ret = RK_MPI_VI_SetChnAttr(0, channelId, &vi_chn_attr);
	ret |= RK_MPI_VI_EnableChn(0, channelId);
	if (ret) {
		printf("ERROR: create VI error! ret=%d\n", ret);
		return ret;
	}
	RK_MPI_VI_StartPipe(0);
	return ret;
}

int venc_init(int chnId, int width, int height, RK_CODEC_ID_E enType) 
{
	printf("%s\n",__func__);
	VENC_RECV_PIC_PARAM_S stRecvParam;
	VENC_CHN_ATTR_S stAttr;
	memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));

	if (enType == RK_VIDEO_ID_AVC) {
		//stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
		//stAttr.stRcAttr.stH264Cbr.u32BitRate = 5* 1024;//5M
		//stAttr.stRcAttr.stH264Cbr.u32Gop = 60;
		stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264VBR;
		stAttr.stRcAttr.stH264Vbr.u32Gop = 30;
		stAttr.stRcAttr.stH264Vbr.u32BitRate = 5* 1024;
		stAttr.stRcAttr.stH264Vbr.u32MaxBitRate  = 8* 1024;
		stAttr.stRcAttr.stH264Vbr.u32MinBitRate = 2*1024;
		
	} else if (enType == RK_VIDEO_ID_HEVC) {
		stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
		stAttr.stRcAttr.stH265Cbr.u32BitRate = 10 * 1024;
		stAttr.stRcAttr.stH265Cbr.u32Gop = 60;
	} else if (enType == RK_VIDEO_ID_MJPEG) {
		stAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
		stAttr.stRcAttr.stMjpegCbr.u32BitRate = 10 * 1024;
	}

	stAttr.stVencAttr.enType = enType;
	stAttr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
	if (enType == RK_VIDEO_ID_AVC)
	stAttr.stVencAttr.u32Profile = H264E_PROFILE_HIGH;
	stAttr.stVencAttr.u32PicWidth = width;
	stAttr.stVencAttr.u32PicHeight = height;
	stAttr.stVencAttr.u32VirWidth = width;
	stAttr.stVencAttr.u32VirHeight = height;
	stAttr.stVencAttr.u32StreamBufCnt = 2;
	stAttr.stVencAttr.u32BufSize = width * height * 3 / 2;
	stAttr.stVencAttr.enMirror = MIRROR_NONE;

	RK_MPI_VENC_CreateChn(chnId, &stAttr);

	memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
	stRecvParam.s32RecvPicNum = -1;
	RK_MPI_VENC_StartRecvFrame(chnId, &stRecvParam);
	
	return 0;
}
void Init_ISP(void)
{
    RK_BOOL multi_sensor = RK_FALSE;	
	const char *iq_dir = "/etc/iqfiles";
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
	//hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
	SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
	SAMPLE_COMM_ISP_Run(0);
}

void Init_VenC(void)
{
  
    
    // venc init
	RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC;
	venc_init(0, 1920, 1080, enCodecType);
    //bind
    MPP_CHN_S src;
    MPP_CHN_S dst;
    src.enModId = RK_ID_VI;
    src.s32ChnId = 0;
    src.s32DevId = 0;

    dst.enModId = RK_ID_VENC;
    dst.s32ChnId = 0;
    dst.s32DevId = 0;
    RK_MPI_SYS_Bind(&src,&dst);//VI通道绑定到VENC通道
}

int main(int argc,char * argv[])
{ 
    system("RkLunch-stop.sh");
    
//1：初始化 ISP
    RK_BOOL multi_sensor = RK_FALSE;	
	const char *iq_dir = "/etc/iqfiles";
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
	//hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
	SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
	SAMPLE_COMM_ISP_Run(0);
    printf("初始化 ISP 成功！\r\n");
//2：初始化 RKMPI
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
		RK_LOGE("rk mpi sys init fail!");
		return -1;
	}
    printf("初始化 MPI 系统成功！\r\n");

    vi_dev_init();
    printf("初始化VI成功");

//3：初始化 VI 
    //FRAME_RATE_CTRL_S   stFrameRate = {}; 
    VI_CHN_ATTR_S viattr={0};
    viattr.bFlip=RK_FALSE;//是否翻转
    viattr.bMirror=RK_FALSE;//镜像
    viattr.enAllocBufType=VI_ALLOC_BUF_TYPE_INTERNAL;
    viattr.enCompressMode=COMPRESS_MODE_NONE;//编码压缩模式
    viattr.enDynamicRange=DYNAMIC_RANGE_SDR10;//动态范围
    viattr.enPixelFormat = RK_FMT_YUV420SP;//NV12
    //viattr.stFrameRate={30,1};//帧率
    viattr.stIspOpt.enCaptureType=VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE;
    viattr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;//DMA加速
    viattr.stIspOpt.stMaxSize = {1920,1080};
    viattr.stIspOpt.stWindow = {0,0,1920,1080};
    viattr.stIspOpt.u32BufCount = 3;
    viattr.stIspOpt.u32BufSize = 1920*1080*2;
    viattr.stSize.u32Height = 1080;
    viattr.stSize.u32Width = 1920;
    viattr.u32Depth = 3;

    int ret = RK_MPI_VI_SetChnAttr(0, 0, &viattr);
	ret |= RK_MPI_VI_EnableChn(0, 0);
	if (ret) {
		printf("ERROR: create VI error! ret=%d\n", ret);
		return ret;
	}
    
    Init_VenC();
    // rtsp init	
	rtsp_demo_handle g_rtsplive = NULL;
	rtsp_session_handle g_rtsp_session;
	g_rtsplive = create_rtsp_demo(554);
	g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/test");
	rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
	rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());


    VIDEO_FRAME_INFO_S stViFrame;
    VENC_STREAM_S stFrame;	
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
    int fps_count = 0;
    int width = 1920;
    int height = 1080;
    //h264_frame	
	
	RK_U64 H264_PTS = 0;
	RK_U32 H264_TimeRef = 0; 

	// Create Pool
	MB_POOL_CONFIG_S PoolCfg;
	memset(&PoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
	PoolCfg.u64MBSize = width * height * 3 ;
	PoolCfg.u32MBCnt = 1;
	PoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
	//PoolCfg.bPreAlloc = RK_FALSE;
	MB_POOL src_Pool = RK_MPI_MB_CreatePool(&PoolCfg);
	printf("Create Pool success !\n");	

	// Get MB from Pool 
	MB_BLK src_Blk = RK_MPI_MB_GetMB(src_Pool, width * height * 3, RK_TRUE);
	
	// Build h264_frame
	VIDEO_FRAME_INFO_S h264_frame;
	h264_frame.stVFrame.u32Width = width;
	h264_frame.stVFrame.u32Height = height;
	h264_frame.stVFrame.u32VirWidth = width;
	h264_frame.stVFrame.u32VirHeight = height;
	h264_frame.stVFrame.enPixelFormat =  RK_FMT_RGB888; 
	h264_frame.stVFrame.u32FrameFlag = 160;
	h264_frame.stVFrame.pMbBlk = src_Blk;
	unsigned char *data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);
	cv::Mat frame(cv::Size(width,height),CV_8UC3,data);
	
    char  fps_text[16];
    int fps = 0;
    while(1)
    { 
        h264_frame.stVFrame.u32TimeRef = H264_TimeRef++;
		h264_frame.stVFrame.u64PTS = TEST_COMM_GetNowUs(); 
		int s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
		if(s32Ret == RK_SUCCESS)
		{
			void *vi_data = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);

			//创建了一个openCV Mat，用于存放转换后的YUV420SP数据
			cv::Mat yuv420sp(height + height / 2, width, CV_8UC1, vi_data);
			//创建了一个openCV Mat，用于存放转换后的BGR数据
			cv::Mat bgr(height, width, CV_8UC3, data);	
					
			cv::cvtColor(yuv420sp, bgr, cv::COLOR_YUV420sp2BGR);
			cv::resize(bgr, frame, cv::Size(width ,height), 0, 0, cv::INTER_LINEAR);
			
			//在图像上绘制FPS文字
			sprintf(fps_text,"fps = %.2f",fps);		
            cv::putText(frame,fps_text,
							cv::Point(40, 40),
							cv::FONT_HERSHEY_SIMPLEX,1,
							cv::Scalar(0,255,0),2);
			
		}
		memcpy(data, frame.data, width * height * 3);
		
		// encode H264	
		RK_MPI_VENC_SendFrame(0,  &h264_frame ,-1);
		//RK_MPI_VENC_SendFrame(0, &stViFrame, -1);  //直接送 VI 的原始 YUV 帧
	
		// rtsp
		s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, -1);	
		if(s32Ret == RK_SUCCESS) {
			if(g_rtsplive && g_rtsp_session) {
				//printf("len = %d PTS = %d \n",stFrame.pstPack->u32Len, stFrame.pstPack->u64PTS);	
				void *pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
				rtsp_tx_video(g_rtsp_session, (uint8_t *)pData, stFrame.pstPack->u32Len,
							  stFrame.pstPack->u64PTS);
				rtsp_do_event(g_rtsplive);
			}
			RK_U64 nowUs = TEST_COMM_GetNowUs();
			fps = (float) 1000000 / (float)(nowUs - h264_frame.stVFrame.u64PTS);			
		}
		// release frame 
		s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
		}
		s32Ret = RK_MPI_VENC_ReleaseStream(0, &stFrame);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", s32Ret);
		}
    }
    return 0;
}
