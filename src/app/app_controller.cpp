#include "app/app_controller.h"
extern "C"
{
    #include "infra/logger.h"
}

int app_controller_init()//初始化各种函数
{
    log_init("camera.log",LOG_LEVEL_DEBUG);//camera.log为日志(LOGI打印的信息)存储的文件夹
    
    int ret = core_video_init();//视频采集模块初始化

    if(ret != 0)
    {
        LOGI("core_video_init failed!");//失败打印
        return -1;
    }
    
    LOGI("core_video_init successful!");//成功打印
    
    return 0;
}
int app_controller_run()//执行初始化后的函数
{

    int ret = core_video_run();//视频采集模块执行
    if(ret != 0)
    {
        LOGI("core_video_run failed!");//失败
        return -1;
    }
    LOGI("core_video_run successful!");//成功

    return 0;
}
