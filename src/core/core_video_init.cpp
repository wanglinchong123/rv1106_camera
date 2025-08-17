#include "core/core_video_init.h"


int core_video_init(void)
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
    
    return 0;
}
  