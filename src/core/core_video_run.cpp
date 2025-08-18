#include "core/core_video_run.h"
#include "signal.h"


static volatile bool g_exit = false;

void sig_handler(int sig) {
    printf("catch signal %d, exit...\n", sig);
    g_exit = true;
}

using namespace cv;			// OpenCV 的 C++ 命名空间

RK_U64 TEST_COMM_GetNowUs() // 获取当前时间，精确到微秒（µs）
{
	struct timespec time = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
}

int core_video_run(void)
{
	// 注册信号，支持 Ctrl+C 退出
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

	//RTSP会话建立
	rtsp_demo_handle g_rtsplive = NULL;
	rtsp_session_handle g_rtsp_session;	// RTSP流会话
	g_rtsplive = create_rtsp_demo(554);	// 554端口
	g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/test");		// 创建了一个​RTSP会话,路径为/live/test​​
	rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);	// 设置这个RTSP会话的视频编码格式为：H.264
	rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime()); // 同步视频的时间戳

	VIDEO_FRAME_INFO_S stViFrame;// 从摄像头（VI）获取的原始帧数据
	VENC_STREAM_S stFrame;		// 从编码器（VENC）获取的编码后数据流（H.264）
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S)); // 为编码器输出的VENC数据包分配内存

	int fps_count = 0;
	RK_U64 last_time = 0;

	// 视频分辨率
	int width = 1920;
	int height = 1080;

	// 定义H.264帧的时间戳变量
	RK_U64 H264_PTS = 0;
	RK_U32 H264_TimeRef = 0;

	// 创建一个可被硬件访问的DMA内存池，分配一块用于存 BGR 图像的 MB
	//这块内存会被 OpenCV 直接当作 Mat 的 data 使用
	MB_POOL_CONFIG_S PoolCfg;
	memset(&PoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
	PoolCfg.u64MBSize = width * height * 3; // 用于存放 BGR 数据
	PoolCfg.u32MBCnt = 1;
	PoolCfg.enAllocType = MB_ALLOC_TYPE_DMA; // 使用DMA内存，通常用于摄像头、编码器等硬件直接访问
	// PoolCfg.bPreAlloc = RK_FALSE;
	MB_POOL src_Pool = RK_MPI_MB_CreatePool(&PoolCfg); // 从内存池中申请一块内存，用于存放图像数据（BGR）
	printf("Create Pool success !\n");

	// Get MB from Pool
	MB_BLK src_Blk = RK_MPI_MB_GetMB(src_Pool, width * height * 3, RK_TRUE);

	// 初始化一个视频帧信息结构体h264_frame，描述一帧图像
	VIDEO_FRAME_INFO_S h264_frame;
	h264_frame.stVFrame.u32Width = width;
	h264_frame.stVFrame.u32Height = height;
	h264_frame.stVFrame.u32VirWidth = width;
	h264_frame.stVFrame.u32VirHeight = height;
	h264_frame.stVFrame.enPixelFormat = RK_FMT_RGB888; // RGB像素格式
	h264_frame.stVFrame.u32FrameFlag = 160;
	h264_frame.stVFrame.pMbBlk = src_Blk;

	// 将 DMA 内存块转换为虚拟地址，用 OpenCV 的 cv::Mat封装，方便做图像处理（比如缩放、绘图等）
	unsigned char *data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);
	cv::Mat frame(cv::Size(width, height), CV_8UC3, data);

	char fps_text[16];
	float fps = 0;

	// 持续采集和编码视频（采集 → 处理 → 编码 → 推流）
	while (!g_exit)
	{
		// 每一帧开始时，设置时间戳和参考时间
		h264_frame.stVFrame.u32TimeRef = H264_TimeRef++;
		h264_frame.stVFrame.u64PTS = TEST_COMM_GetNowUs();

		RK_U64 nowUs = TEST_COMM_GetNowUs();//获取当前时间

		fps_count++;

		if(last_time == 0)//视频刚打开
		{
			last_time = nowUs;
		}
		else if((nowUs - last_time) >= 1000000)//单位为微秒
		{
			fps = (float)fps_count*1000000/(nowUs - last_time);
			
			fps_count = 0;
			last_time = nowUs;

		}

		// 从摄像头通道获取一帧原始数据到stViFrame，再转换为BGR，再缩放/显示
		int s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
		if (s32Ret == RK_SUCCESS)
		{
			void *vi_data = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);

			// 创建了一个openCV Mat，用于存放转换后的YUV420SP数据
			cv::Mat yuv420sp(height + height / 2, width, CV_8UC1, vi_data);
			// 创建了一个openCV Mat，用于存放转换后的BGR数据
			cv::Mat bgr(height, width, CV_8UC3, data);
			cv::cvtColor(yuv420sp, bgr, cv::COLOR_YUV420sp2BGR);
			cv::resize(bgr, frame, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);

			// 在图像上绘制FPS文字
			sprintf(fps_text, "fps = %.2f", fps);
			cv::putText(frame, fps_text,
						cv::Point(40, 40),
						cv::FONT_HERSHEY_SIMPLEX, 1,
						cv::Scalar(0, 255, 0), 2);
		}
		else
		{
			printf("RK_MPI_VI_GetChnFrame失败\n");
			continue;
		}
		memcpy(data, frame.data, width * height * 3); // data中已经是​带FPS的BGR图像

		//把上面那块BGR/“声明为RGB888”的帧送给编码器通道（VENC）0--编码为H.264
		if (RK_MPI_VENC_SendFrame(0, &h264_frame, -1) != 0)
		{
			printf("RK_MPI_VENC_SendFrame失败\n");
			continue;
		}

		// rtsp
		s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, -1);

		if (s32Ret == RK_SUCCESS)
		{
			printf("RK_MPI_VENC_GetStream 成功\n");
			if (g_rtsplive && g_rtsp_session)
			{
				printf("len = %d PTS = %d \n", stFrame.pstPack->u32Len, stFrame.pstPack->u64PTS);
				void *pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
				rtsp_tx_video(g_rtsp_session, (uint8_t *)pData, stFrame.pstPack->u32Len,
							  stFrame.pstPack->u64PTS);
				rtsp_do_event(g_rtsplive);
			}
		}
		else
		{
			printf("RK_MPI_VENC_GetStream失败\n");
		}
		// release frame
		s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
		if (s32Ret != RK_SUCCESS)
		{
			RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
		}
		s32Ret = RK_MPI_VENC_ReleaseStream(0, &stFrame);
		if (s32Ret != RK_SUCCESS)
		{
			RK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", s32Ret);
		}
	}
	//释放资源
	if (stFrame.pstPack) {
        free(stFrame.pstPack);
        stFrame.pstPack = NULL;
    }
    if (g_rtsplive && g_rtsp_session) {
        rtsp_del_session(g_rtsp_session);
        rtsp_del_demo(g_rtsplive);
    }
    if (src_Pool) {
        RK_MPI_MB_DestroyPool(src_Pool);
    }
    printf("视频推流开辟资源释放完成\n");

	return 0;
}