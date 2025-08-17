#include "driver/vi_init.h"


//1.VI设备初始化
int vi_dev_init(void) 
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

//2：初始化VI通道（视频输入）
int vi_init(void) 
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
    viattr.stFrameRate={30,1};//帧率

	//设置ISP相关参数（负责对采集到的原始图像数据进行​​“美颜”和“优化”​​）
    viattr.stIspOpt.enCaptureType=VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE;//视频捕获模式
    // viattr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;//内存类型：DMA加速（硬件加速）
	viattr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_MMAP;//内存类型：DMA加速（硬件加速）
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

//3.初始化指定参数的VI通道
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
