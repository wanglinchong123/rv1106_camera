cmake_minimum_required(VERSION 3.22.1)#版本
project(camera)#工程名

set(CMAKE_CXX_STANDARD 14)
#设置C++ 编译的参数(根据需要自己设置)
set(CMAKE_CXX_FLAGS "-O2 -g")
set(CMAKE_C_FLAGS "-std=c99 -Wall -O0 -g3 -Wall -fmessage-length=0")

#头文件搜索路径
include_directories(
    /home/lyx/luckfox-pico/media/rockit/rockit/mpi/example/include
    /home/lyx/code/xyd_source/include
    /home/lyx/code/xyd_source/include/opencv4
    /home/lyx/code/xyd_source/include/rkaiq/uAPI2
    /home/lyx/code/xyd_source/include/rockchip
    /home/lyx/code/xyd_source/include/rkaiq/
    /home/lyx/code/xyd_source/include/rkaiq/common
    /home/lyx/code/xyd_source/include/rkaiq/xcore
    /home/lyx/code/xyd_source/include/rkaiq/algos
    /home/lyx/code/xyd_source/include/rkaiq/iq_parser
    /home/lyx/code/xyd_source/include/rkaiq/iq_parser_v2
    ${CMAKE_CURRENT_LIST_DIR}/
    ${CMAKE_CURRENT_LIST_DIR}/include
)

#库文件搜索路径
#去这些目录下找 .so 或 .a 动态/静态库文件​​
link_directories(
    /home/lyx/code/xyd_source/lib
    /home/lyx/luckfox-pico/sysdrv/source/buildroot/buildroot-2023.02.6/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/lib
    /home/lyx/luckfox-pico/sysdrv/source/buildroot/buildroot-2023.02.6/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/lib
    ${CMAKE_CURRENT_LIST_DIR}/
)

#创建的C文件地址
add_executable(camera
    src/main.cpp

    src/infra/logger.c

    src/app/app_controller.cpp

    src/driver/isp_driver.cpp

    src/driver/vi_driver.cpp

    src/driver/venc_driver.cpp

    src/core/core_video_init.cpp

    src/core/core_video_run.cpp
    

    # /home/lyx/luckfox-pico/media/rockit/rockit/mpi/example/common/test_comm_argparse.cpp
)

target_link_libraries(camera
    pthread
    dl
    rkaiq
    rockchip_mpp
    rockit
    rtsp
    rknnmrt
    rockit_tiny
    sample_comm
    rga
    rockit_full
    rockiva
    m
    avformat
    avcodec
    avdevice
    avutil
    #opencv_core 太神奇了
    opencv_features2d
    opencv_highgui
    opencv_imgproc
    opencv_photo
    opencv_video
    opencv_imgproc
    opencv_core
)