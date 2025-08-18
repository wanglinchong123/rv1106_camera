#pragma once

#include "stdio.h"
#include "rk_comm_venc.h"
#include "string.h"
#include "rk_mpi_venc.h"
#include "rk_mpi_sys.h"

int venc_init(int chnId, int width, int height, RK_CODEC_ID_E enType);
void Init_VenC();