#include "driver/isp_init.h"
#include "sample_comm.h"


int isp_init()//初始化ISP
{
    RK_BOOL multi_sensor = RK_FALSE;//不使用多传感器	
	const char *iq_dir = "/etc/iqfiles";//图像质量参数文件路径
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
	//hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
	SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
	//运行ISP
	SAMPLE_COMM_ISP_Run(0);
}
