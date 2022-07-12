
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <fstream>
#include <regex>
#include <chrono>
#include <thread>
#include <memory>

#include <math.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <signal.h>
#include <arpa/inet.h>


#include "comet_wsf_regctrl.hh"
#include "comet_wsf_regdefine.hh"

#ifndef DEBUG_OFFLINE
#define DEBUG_OFFLINE 0
#endif


namespace{
  std::string CStringToHexString(const char *bin, int len){
    constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                               '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    const unsigned char* data = (const unsigned char*)(bin);
    std::string s(len * 2, ' ');
    for (int i = 0; i < len; ++i) {
      s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
      s[2 * i + 1] = hexmap[data[i] & 0x0F];
    }
    return s;
  }

  std::string StringToHexString(const std::string bin){
    return CStringToHexString(bin.data(), bin.size());
  }
}


namespace comet_wsf{
  comet_regctrl::comet_regctrl(){
    off_t addrbase_phy = R_DAQB_PBASE;
    pR_DAQB_VBASE32 = 0;
    map_mem_block(&fd, addrbase_phy, D_DAQB_PSIZE, &pR_DAQB_VBASE32);
    if(pR_DAQB_VBASE32==0){
      std::fprintf(stderr, "Memory mapped failed\n");
      ummap_mem_block(&fd, &pR_DAQB_VBASE32);
      throw;
    }
  }

  comet_regctrl::~comet_regctrl(){
    ummap_mem_block(&fd, &pR_DAQB_VBASE32);
  }



  void comet_regctrl::map_mem_block(int* p_fd, uint32_t paddr, uint32_t psize, uint32_t** pp_vbase32){
    if(DEBUG_OFFLINE){
      *p_fd = 0;
      *pp_vbase32 = new uint32_t[(psize+3)/4];
      debug_print("emulate a memory block for offline debug.\n");
      return;
    }

    if ((*p_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0 ) {
      std::fprintf(stderr, "Error opening file. \n");
      throw;
    }
    size_t page_size = getpagesize();
    off_t offset_in_page = paddr & (page_size - 1);
    size_t mapped_size = page_size * ( (offset_in_page + psize)/page_size + 1);
    void *map_base = mmap(NULL,
                          mapped_size,
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED,
                          *p_fd,
                          paddr & ~(off_t)(page_size - 1));

    if (map_base == MAP_FAILED){
      std::fprintf(stderr, "Memory mapped failed\n");
      throw;
    }
    char* pchar_base = (char*)map_base + offset_in_page;
    *pp_vbase32 =  reinterpret_cast<uint32_t *>(pchar_base);
  }


  void comet_regctrl::ummap_mem_block(int* p_fd, uint32_t** pp_vbase32){
    if(DEBUG_OFFLINE){
      *p_fd = 0;
      delete [] (*pp_vbase32);
      *pp_vbase32 = 0;
      debug_print("close emulated a memory block under offline debug.\n");
      return;
    }
    close(*p_fd);
    *p_fd = 0;
    *pp_vbase32 = 0;
    return;
  }



  void comet_regctrl::daqb_reg_write(uint32_t o, uint32_t d, uint32_t* vbase){
    std::printf("writing DAQB register:  %#010x @ %#010x \n", d, o);

    uint32_t *p_target = (uint32_t*)(((char*)vbase) +o);
    *p_target = d;
  }


  uint32_t comet_regctrl::daqb_reg_read(uint32_t o, uint32_t* vbase){
    uint32_t data =  *((uint32_t*)(((char*)vbase) +o));
    if(o==R_DAQB_CMD_ACK){
      std::printf("monitoring DAQB ACK:  %#010x @ %#010x \n", data, o);
    }
    else{
      std::printf("read DAQB register:  %#010x @ %#010x \n", data, o);
    }
    return data;
  }

  void comet_regctrl::daqb_reg_write(uint32_t o, uint32_t d){
    uint32_t *vbase = pR_DAQB_VBASE32;
    daqb_reg_write(o, d, vbase);
  }

  uint32_t comet_regctrl::daqb_reg_read(uint32_t o){
    uint32_t *vbase = pR_DAQB_VBASE32;
    return daqb_reg_read(o, vbase);
  }


  void comet_regctrl::feb_cmd_write(uint16_t a, uint16_t d){
    uint32_t cmd = ((uint32_t(a))<<16) + d;
    std::printf("writing FEB register: %#010x @ %#010x \n", d, a);
    daqb_reg_write(R_DAQB_CMD_HADDR_LDATA, cmd);
  }


  uint32_t comet_regctrl::daqb_read_last_ack_cnt(){
    return daqb_reg_read(R_DAQB_CMD_ACK);
  }

  void comet_regctrl::daqb_wait_for_ack_cnt(uint32_t last_cnt){
    if(DEBUG_OFFLINE){
      debug_print("offline emulation: increase R_DAQB_CMD_ACK, set it to %u\n", last_cnt+1);
      daqb_reg_write(R_DAQB_CMD_ACK, last_cnt+1);
    }

    auto tp_start = std::chrono::steady_clock::now();
    auto tp_timeout = tp_start + 2s;
    for(uint32_t cnt_new = daqb_reg_read(R_DAQB_CMD_ACK); cnt_new ==last_cnt; ){
      if(std::chrono::steady_clock::now()>tp_timeout){
        std::printf("timetout: last ack cnt %#010x,  %#010x \n", last_cnt, cnt_new);
        throw;
      }
      std::this_thread::sleep_for(20ms);
      cnt_new = daqb_reg_read(R_DAQB_CMD_ACK);
    }
  }

  double comet_regctrl::feb_read_new_temp(){
    uint32_t last_temp_ascii = daqb_reg_read(R_DAQB_TEMP);
    uint32_t last_cmd_ack_cnt = daqb_read_last_ack_cnt();
    feb_cmd_write(R_FEB_TEMP_FETCH, 1);
    daqb_wait_for_ack_cnt(last_cmd_ack_cnt);
    uint32_t temp_ascii = daqb_reg_read(R_DAQB_TEMP);
    if(DEBUG_OFFLINE){
      temp_ascii = 0x42383235;
    }
    std::printf("temp raw new %#010x, last %#010x \n", temp_ascii, last_temp_ascii);

    if(temp_ascii==0 || temp_ascii==-1){
      std::printf("temp readback is incorrect\n");
      return 0;
    }

    std::string temp_str(reinterpret_cast<const char*>(&temp_ascii), 4);
    std::string temp_str_revert(temp_str.rbegin(), temp_str.rend());
    std::printf("temp hex 0X%s\n", temp_str_revert.c_str());

    uint32_t temp_div =  std::stoul(temp_str_revert, 0, 16);
    double temp = (temp_div * 0.00001907-1.035)/(-0.0055);

    std::printf("temp   (%u * 0.00001907 - 1.035)/(-0.0055) = %f Celsius \n", temp_div, temp);

    return temp;
  }


  double comet_regctrl::feb_read_hv(){
    uint32_t hv_ascii = daqb_reg_read(R_DAQB_HVOLT);
    std::printf("hvolt raw %#010x\n", hv_ascii);

    if(hv_ascii==0 || hv_ascii==-1){
      std::printf("hvolt readback is incorrect\n");
      return 0;
    }
    std::string hv_str(reinterpret_cast<const char*>(&hv_ascii), 4);
    std::string hv_str_revert(hv_str.rbegin(), hv_str.rend());

    std::printf("hvolt hex 0X%s\n", hv_str_revert.c_str());
    uint32_t hv_div =  std::stoul(hv_str_revert, 0, 16);
    double hv = hv_div * U_HVOLT_V_PER;

    std::printf("hvolt  %u * U_HVOLT_V_PER = %f Volt \n", hv_div, hv);

    return hv;
  }


  void comet_regctrl::feb_set_hv_voltage(double v){
    uint16_t hvseti = abs(v/U_HVOLT_V_PER);
    std::printf("hvolt conf voltage:  %f / U_HVOLT_V_PER  = %u, hex %#06x \n", v, hvseti, hvseti);

    char hvset_h = ((hvseti>>8) & 0x00ff);
    char hvset_l = (hvseti & 0x00ff);
    std::string hvset_hs = CStringToHexString(&hvset_h, 1);
    std::string hvset_ls = CStringToHexString(&hvset_l, 1);
    uint16_t hvset_hi = ((uint16_t(uint8_t(hvset_hs[0])))<<8) + (uint16_t(uint8_t(hvset_hs[1])));
    uint16_t hvset_li = ((uint16_t(uint8_t(hvset_ls[0])))<<8) + (uint16_t(uint8_t(hvset_ls[1])));
    std::printf("hvolt conf VH, VL:  %#06x, %#06x\n", hvset_hi, hvset_li);

    uint32_t last_ack_cnt;
    last_ack_cnt = daqb_read_last_ack_cnt();
    feb_cmd_write(R_FEB_HVOLT_REF_VH, hvset_hi);
    daqb_wait_for_ack_cnt(last_ack_cnt);

    last_ack_cnt = daqb_read_last_ack_cnt();
    feb_cmd_write(R_FEB_HVOLT_REF_VL, hvset_li);
    daqb_wait_for_ack_cnt(last_ack_cnt);


    std::printf("hvolt reading new temp\n");
    double temp = feb_read_new_temp();
    uint16_t tempref_div = abs( (1.035+ (temp*(-0.0055)))/0.00001907 );
    std::printf("hvolt conf tempref:  (1.035+ ( %f *(-0.0055)))/0.00001907) = %u, hex %#06x \n", temp, tempref_div, tempref_div);

    char tempref_h = ((tempref_div>>8) & 0x00ff);
    char tempref_l = (tempref_div & 0x00ff);
    std::string tempref_hs = CStringToHexString(&tempref_h, 1);
    std::string tempref_ls = CStringToHexString(&tempref_l, 1);
    uint16_t tempref_hi = ((uint16_t(uint8_t(tempref_hs[0])))<<8) + (uint16_t(uint8_t(tempref_hs[1])));
    uint16_t tempref_li = ((uint16_t(uint8_t(tempref_ls[0])))<<8) + (uint16_t(uint8_t(tempref_ls[1])));
    std::printf("hvolt conf TH, TL:  %#06x, %#06x, %#06x, %#06x \n", tempref_hi, tempref_li);

    last_ack_cnt = daqb_read_last_ack_cnt();
    feb_cmd_write(R_FEB_HVOLT_REF_TH, tempref_hi);
    daqb_wait_for_ack_cnt(last_ack_cnt);

    last_ack_cnt = daqb_read_last_ack_cnt();
    feb_cmd_write(R_FEB_HVOLT_REF_TL, tempref_li);
    daqb_wait_for_ack_cnt(last_ack_cnt);

    char hv_checksum = 0x00ff & (   0x0e +
                                    ((hvset_hi>>8)&0xff) + (hvset_hi&0xff) +
                                    ((hvset_li>>8)&0xff) + (hvset_li&0xff) +
                                    ((tempref_hi>>8)&0xff) + (tempref_hi&0xff) +
                                    ((tempref_li>>8)&0xff) + (tempref_li&0xff) );

    std::string hv_checksum_s = CStringToHexString(&hv_checksum, 1);
    std::printf("hvolt conf CHECKSUM: SUM %#06x\n,  HEX 0X%s ", (uint16_t(uint8_t(hv_checksum))), hv_checksum_s.c_str());
    uint16_t hv_checksum_i = ((uint16_t(uint8_t(hv_checksum_s[0])))<<8) + (uint16_t(uint8_t(hv_checksum_s[1])));
    std::printf("hvolt conf CHECKSUM:  %#06x\n", hv_checksum_i);

    last_ack_cnt = daqb_read_last_ack_cnt();
    feb_cmd_write(R_FEB_HVOLT_CHECKSUM, hv_checksum_i);
    daqb_wait_for_ack_cnt(last_ack_cnt);


    last_ack_cnt = daqb_read_last_ack_cnt();
    feb_cmd_write(R_FEB_HVOLT_CONF_PUSH, 1);
    daqb_wait_for_ack_cnt(last_ack_cnt);
  }

  void comet_regctrl::feb_set_dac_voltage(uint32_t ch, double v){
    uint32_t v_raw = abs(v/U_DAC_V_PER);
    uint32_t data_raw = ((ch & 0xf) << 12) + (v_raw & 0xfff);

    uint32_t last_ack_cnt;
    last_ack_cnt = daqb_read_last_ack_cnt();
    feb_cmd_write(R_FEB_DAC_CONF, data_raw);
    daqb_wait_for_ack_cnt(last_ack_cnt);

    last_ack_cnt = daqb_read_last_ack_cnt();
    feb_cmd_write(R_FEB_DAC_CONF_PUSH, 1);
    daqb_wait_for_ack_cnt(last_ack_cnt);
  }


  void comet_regctrl::feb_set_asic_raw(uint32_t ch, uint16_t data){
    if(ch>1){
      std::printf("Error: asic%u does not exist.\n", ch);
      return;
    }

    uint32_t last_ack_cnt;
    last_ack_cnt = daqb_read_last_ack_cnt();
    if(ch==0){
      feb_cmd_write(R_FEB_ASIC0_CONF, data);
    }
    else{
      feb_cmd_write(R_FEB_ASIC1_CONF, data);
    }
    daqb_wait_for_ack_cnt(last_ack_cnt);

    last_ack_cnt = daqb_read_last_ack_cnt();
    if(ch==0){
      feb_cmd_write(R_FEB_ASIC0_CONF_PUSH, 1);
    }
    else{
      feb_cmd_write(R_FEB_ASIC1_CONF_PUSH, 1);
    }
    daqb_wait_for_ack_cnt(last_ack_cnt);
  }

  void comet_regctrl::daq_start_run(){
    daqb_reg_write(R_DAQB_DATA_RUN, 1);

    uint32_t last_ack_cnt;
    last_ack_cnt = daqb_read_last_ack_cnt();
    feb_cmd_write(R_FEB_DATA_RUN, 1);
    daqb_wait_for_ack_cnt(last_ack_cnt);
  }


  void comet_regctrl::daq_stop_run(){
    uint32_t last_ack_cnt;
    last_ack_cnt = daqb_read_last_ack_cnt();
    feb_cmd_write(R_FEB_DATA_RUN, 1);
    daqb_wait_for_ack_cnt(last_ack_cnt);

    daqb_reg_write(R_DAQB_DATA_RUN, 0);
  }

  void comet_regctrl::daq_reset(){
    uint32_t last_ack_cnt;
    last_ack_cnt = daqb_read_last_ack_cnt();
    feb_cmd_write(R_FEB_RESET,1);
    daqb_wait_for_ack_cnt(last_ack_cnt);

    daqb_reg_write(R_DAQB_DATA_RESET,1);
    std::this_thread::sleep_for(100ms);
  }

  void comet_regctrl::daq_conf_default(){

    std::vector<double>default_dac_v= {1, 1, 0, 0, 2.3, 2.3, 1, 1};
    double default_hvolt_v= 54;
    std::printf("conf default: DAC [%f, %f, %f, %f, %f, %f, %f, %f] Volt ,  HVOLT [%f] Volt \n",
                default_dac_v[0],default_dac_v[1],default_dac_v[2],default_dac_v[3],
                default_dac_v[4],default_dac_v[5],default_dac_v[6],default_dac_v[7],
                default_hvolt_v);

    feb_set_dac_voltage(0, default_dac_v[0]);
    feb_set_dac_voltage(1, default_dac_v[1]);
    feb_set_dac_voltage(2, default_dac_v[2]);
    feb_set_dac_voltage(3, default_dac_v[3]);
    feb_set_dac_voltage(4, default_dac_v[4]);
    feb_set_dac_voltage(5, default_dac_v[5]);
    feb_set_dac_voltage(6, default_dac_v[6]);
    feb_set_dac_voltage(7, default_dac_v[7]);
    feb_set_hv_voltage(54);
  }

}
