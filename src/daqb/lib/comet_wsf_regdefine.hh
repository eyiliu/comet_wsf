#pragma once

#define R_DAQB_PBASE    0x43c00000
#define D_DAQB_PSIZE    0x100

#define R_DAQB_CMD_HADDR_LDATA  0x0
#define R_DAQB_DATA_RESET       0x4
#define R_DAQB_DATA_RUN         0x8
#define R_DAQB_CMD_ACK          0xc
#define R_DAQB_TEMP             0x10
#define R_DAQB_HVOLT            0x14

#define U_DAC_V_PER             0.0008059
#define U_HVOLT_V_PER           0.001812

#define R_FEB_RESET             0x0000
#define R_FEB_DAC_CONF          0x0001
#define R_FEB_ASIC0_CONF        0x0002
#define R_FEB_ASIC1_CONF        0x0003
#define R_FEB_HVOLT_REF_VH      0x0004
#define R_FEB_HVOLT_REF_VL      0x0005
#define R_FEB_HVOLT_REF_TH      0x0006
#define R_FEB_HVOLT_REF_TL      0x0007
#define R_FEB_HVOLT_CHECKSUM    0x0008
#define R_FEB_DAC_CONF_PUSH     0x0009
#define R_FEB_ASIC0_CONF_PUSH   0x000a
#define R_FEB_ASIC1_CONF_PUSH   0x000b
#define R_FEB_HVOLT_CONF_PUSH   0x000c
#define R_FEB_DATA_RUN          0x000d
#define R_FEB_TEMP_FETCH        0x000e

