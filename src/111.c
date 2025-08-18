// // ---- 1. 从摄像头获取一帧原始 YUV 数据 ----
// int s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
// if (s32Ret == RK_SUCCESS)
// {
//     void *vi_data = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);

//     // ---- 2. 将 YUV 转为 OpenCV BGR Mat ----
//     cv::Mat yuv420sp(height + height / 2, width, CV_8UC1, vi_data);
//     cv::Mat bgr(height, width, CV_8UC3, data);		
//     cv::cvtColor(yuv420sp, bgr, cv::COLOR_YUV420sp2BGR);
//     cv::resize(bgr, frame, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);

//     // ---- 3. 计算 FPS 并在 BGR 图像上绘制文字（关键！先绘制）----
//     frame_count++;
//     double now_time = (double)cv::getTickCount();
//     double time_diff = (now_time - prev_time) / cv::getTickFrequency();

//     if (time_diff >= 1.0) {
//         fps = frame_count / time_diff;
//         frame_count = 0;
//         prev_time = now_time;
//     }

//     // 绘制 FPS 到 BGR 图像
//     sprintf(fps_text, "fps = %.2f", fps);
//     cv::putText(frame, fps_text,
//                 cv::Point(40, 40),                  // 文字位置
//                 cv::FONT_HERSHEY_SIMPLEX,           // 字体
//                 1.0,                                  // 字体大小
//                 cv::Scalar(0, 255, 0),              // 绿色
//                 2);                                   // 线宽

//     // ---- 4. 将带 FPS 的 BGR 图像转为 YUV420SP（编码器需要的格式）----
//     cv::Mat yuv420p_mat(height * 3 / 2, width, CV_8UC1);
//     cv::cvtColor(frame, yuv420p_mat, cv::COLOR_BGR2YUV_I420);

//     // ---- 5. 分配一个 DMA 内存块用于存放 YUV，并拷贝数据 ----
//     MB_BLK processed_mb = RK_MPI_MB_GetMB(pool, mb_cfg.u64MBSize, RK_TRUE);
//     if (processed_mb == nullptr) {
//         printf("ERROR: RK_MPI_MB_GetMB() failed（可能内存不足）\n");
//     } else {
//         // 获取虚拟地址并拷贝 YUV 数据
//         unsigned char *dst_yuv = (unsigned char *)RK_MPI_MB_Handle2VirAddr(processed_mb);
//         memcpy(dst_yuv, yuv420p_mat.data, mb_cfg.u64MBSize);

//         // ---- 6. 构造编码器帧结构 VIDEO_FRAME_INFO_S ----
//         processed_frame.stVFrame.u32Width = width;
//         processed_frame.stVFrame.u32Height = height;
//         processed_frame.stVFrame.u32VirWidth = width;
//         processed_frame.stVFrame.u32VirHeight = height;
//         processed_frame.stVFrame.enPixelFormat = RK_FMT_YUV420SP; // 与 VENC 配置一致
//         processed_frame.stVFrame.pMbBlk = processed_mb;
//         processed_frame.stVFrame.u64PTS = stViFrame.stVFrame.u64PTS;

//         // ---- 7. 送 VENC 进行编码 ----
//         int venc_ret = RK_MPI_VENC_SendFrame(0, &processed_frame, -1);
//         if (venc_ret != RK_SUCCESS) {
//             printf("ERROR: RK_MPI_VENC_SendFrame failed: 0x%x\n", venc_ret);
//         }

//         // ---- 8. 释放当前 MB ----
//         RK_MPI_MB_ReleaseMB(processed_mb);
//         processed_mb = nullptr;
//     }
// }