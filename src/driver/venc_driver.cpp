#include "driver/venc_init.h"


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
	
    RK_MPI_SYS_Bind(&src,&dst);//绑定VI到VENC通道--把​VI通道0​​绑定到VENC通道0​

}