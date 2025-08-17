#pragma once
#include "string.h"
#include "rk_comm_vi.h"
#include "rk_mpi_vi.h"
#include "stdio.h"
#include "rk_comm_video.h"

int vi_dev_init(void);
int vi_init(void);
int vi_chn_init(int channelId, int width, int height);
