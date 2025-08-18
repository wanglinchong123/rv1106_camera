#include "main.h"
extern "C"
{
    #include "infra/logger.h"
}

int main()
{
    int ret_init = app_controller_init();
    if(ret_init == -1)
    {
        LOGI("app_controller_init failed!");
        return -1;
    }
    LOGI("app_controller_init successfull!");

    int ret_run = app_controller_run();
    if(ret_run == -1)
    {
        LOGI("app_controller_init failed!");
        return -1;
    }
    LOGI("app_controller_init successfull!");

    return 0;
}