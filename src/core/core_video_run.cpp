#include "core/core_video_run.h"



using namespace cv;//OpenCV 的 C++ 命名空间
RK_U64 TEST_COMM_GetNowUs() //获取当前时间，精确到微秒（µs）
{
	struct timespec time = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
}


int core_video_run(void)
{	
	rtsp_demo_handle g_rtsplive = NULL;
	rtsp_session_handle g_rtsp_session;//RTSP流会话
	g_rtsplive = create_rtsp_demo(554);//554端口
	g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/test");// 创建了一个​RTSP会话,路径为/live/test​​
	rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);//设置这个RTSP会话的视频编码格式为：H.264
	rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());//同步视频的时间戳

    VIDEO_FRAME_INFO_S stViFrame;//从摄像头（VI）获取的原始帧数据
    VENC_STREAM_S stFrame;	//从编码器（VENC）获取的编码后数据流（H.264）
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));//为编码器输出的数据包分配内存(VENC)
    int fps_count = 0;

    //视频分辨率
    int width = 1920;
    int height = 1080;

    //定义H.264帧的时间戳变量	
	RK_U64 H264_PTS = 0;
	RK_U32 H264_TimeRef = 0; 

	//创建一个​内存池（Memory Block Pool）​​，用于分配DMA可访问的大块内存，存放图像数据
	MB_POOL_CONFIG_S PoolCfg;
	memset(&PoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
	PoolCfg.u64MBSize = width * height * 3 ;//用于存放 BGR 数据
	PoolCfg.u32MBCnt = 1;
	PoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;//使用DMA内存，通常用于摄像头、编码器等硬件直接访问
	//PoolCfg.bPreAlloc = RK_FALSE;
	MB_POOL src_Pool = RK_MPI_MB_CreatePool(&PoolCfg);//从内存池中申请一块内存，用于存放图像数据（BGR）
	printf("Create Pool success !\n");	

	// Get MB from Pool 
	MB_BLK src_Blk = RK_MPI_MB_GetMB(src_Pool, width * height * 3, RK_TRUE);

    //初始化一个视频帧信息结构体 h264_frame，描述一帧图像
	VIDEO_FRAME_INFO_S h264_frame;
	h264_frame.stVFrame.u32Width = width;
	h264_frame.stVFrame.u32Height = height;
	h264_frame.stVFrame.u32VirWidth = width;
	h264_frame.stVFrame.u32VirHeight = height;
	h264_frame.stVFrame.enPixelFormat =  RK_FMT_RGB888; //RGB像素格式
	h264_frame.stVFrame.u32FrameFlag = 160;
	h264_frame.stVFrame.pMbBlk = src_Blk;
    //将 DMA 内存块转换为虚拟地址，用 OpenCV 的 cv::Mat封装，方便做图像处理（比如缩放、绘图等）
    unsigned char *data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);
	cv::Mat frame(cv::Size(width,height),CV_8UC3,data);
	
    char  fps_text[16];
    int fps = 0;
 
	//持续采集和编码视频（采集 → 处理 → 编码 → 推流）
    while(1)
    {
        //每一帧开始时，设置时间戳和参考时间
        h264_frame.stVFrame.u32TimeRef = H264_TimeRef++;
		h264_frame.stVFrame.u64PTS = TEST_COMM_GetNowUs(); 

        //从摄像头通道获取一帧原始数据到stViFrame；将其转换为YUV420SP格式的OpenCV Mat，再转换为BGR，再缩放/显示
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
		memcpy(data, frame.data, width * height * 3);//data中已经是​带FPS的BGR图像

		// encode H264	
		RK_MPI_VENC_SendFrame(0,  &h264_frame ,-1);
		//RK_MPI_VENC_SendFrame(0, &stViFrame, -1);  //直接送VI的原始 YUV 帧
	
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