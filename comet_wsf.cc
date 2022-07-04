#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <fstream>
#include <regex>
#include <chrono>
#include <thread>
#include <math.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <signal.h>
#include <arpa/inet.h>

#include "linenoise.h"

#ifndef DEBUG_OFFLINE
#define DEBUG_OFFLINE 0
#endif

#ifndef DEBUG_PRINT
#define DEBUG_PRINT 1
#endif
#define debug_print(fmt, ...)                                           \
  do { if (DEBUG_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)

#ifndef INFO_PRINT
#define INFO_PRINT 0
#endif
#define info_print(fmt, ...)                                           \
  do { if (INFO_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)



using std::int8_t;
using std::int16_t;
using std::int32_t;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::size_t;
using namespace std::chrono_literals;


void map_mem_block(int* p_fd, uint32_t paddr, uint32_t psize, uint32_t** pp_vbase32){
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


void ummap_mem_block(int* p_fd, uint32_t** pp_vbase32){
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


uint32_t *pR_DAQB_VBASE32 = 0;


void daqb_reg_write(uint32_t o, uint32_t d, uint32_t* vbase){
  std::printf("writing DAQB register:  %#010x @ %#010x \n", d, o);

  uint32_t *p_target = (uint32_t*)(((char*)vbase) +o);
  *p_target = d;
}


uint32_t daqb_reg_read(uint32_t o, uint32_t* vbase){
  uint32_t data =  *((uint32_t*)(((char*)vbase) +o));
  if(o==R_DAQB_CMD_ACK){
    std::printf("monitoring DAQB ACK:  %#010x @ %#010x \n", data, o);
  }
  else{
    std::printf("read DAQB register:  %#010x @ %#010x \n", data, o);
  }
  return data;
}

void daqb_reg_write(uint32_t o, uint32_t d){
  uint32_t *vbase = pR_DAQB_VBASE32;
  daqb_reg_write(o, d, vbase);
}

uint32_t daqb_reg_read(uint32_t o){
  uint32_t *vbase = pR_DAQB_VBASE32;
  return daqb_reg_read(o, vbase);
}


void feb_cmd_write(uint16_t a, uint16_t d){
  uint32_t cmd = ((uint32_t(a))<<16) + d;
  std::printf("writing FEB register: %#010x @ %#010x \n", d, a);
  daqb_reg_write(R_DAQB_CMD_HADDR_LDATA, cmd);
}


uint32_t daqb_read_last_ack_cnt(){
  return daqb_reg_read(R_DAQB_CMD_ACK);
}

void daqb_wait_for_ack_cnt(uint32_t last_cnt){
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
    std::this_thread::sleep_for(100ms);
    cnt_new = daqb_reg_read(R_DAQB_CMD_ACK);
  }
}

double feb_read_new_temp(){
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


double feb_read_hv(){
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


void feb_set_hv_voltage(double v){
  uint16_t hvseti = lrint(v/U_HVOLT_V_PER);
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
  uint16_t tempref_div = lrint( (1.035+ (temp*(-0.0055)))/0.00001907 );
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



  //todo
  // uint16_t hv_checksum=0;
  // last_ack_cnt = daqb_read_last_ack_cnt();
  // feb_cmd_write(R_FEB_HVOLT_CHECKSUM, hv_checksum);
  // daqb_wait_for_ack_cnt(last_ack_cnt);


  // last_ack_cnt = daqb_read_last_ack_cnt();
  // feb_cmd_write(R_FEB_HVOLT_CONF_PUSH, 1);
  // daqb_wait_for_ack_cnt(last_ack_cnt);

}


void feb_set_dac_voltage(uint32_t ch, double v){
  uint32_t v_raw = lrint(v/U_DAC_V_PER);
  uint32_t data_raw = ((ch & 0xf) << 12) + (v_raw & 0xfff);

  uint32_t last_ack_cnt;
  last_ack_cnt = daqb_read_last_ack_cnt();
  feb_cmd_write(R_FEB_DAC_CONF, data_raw);
  daqb_wait_for_ack_cnt(last_ack_cnt);

  last_ack_cnt = daqb_read_last_ack_cnt();
  feb_cmd_write(R_FEB_DAC_CONF_PUSH, 1);
  daqb_wait_for_ack_cnt(last_ack_cnt);
}


void feb_set_asic_raw(uint32_t ch, uint16_t data){
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




static  const std::string help_usage
(R"(
Usage:
-h : print usage information, and then quit

'help' command in interactive mode provides detail usage information
)"
 );


static  const std::string help_usage_linenoise
(R"(

keyword: help, print, init, reset, quit, daqb, feb, set, get, cmd, hvolt, dac, temp, asic, raw, volt
example:
  1) quit command line
   > quit

  2) get DAQB regiester
   > daqb get raw [nAddress]

  3) set DAQB regiester
   > daqb set raw [nAddress] [nData]

  4) set FEB register
   > feb set raw [nAddress] [nData]

  5) get FEB TEMP module Celsius
   > temp get

  6) set&push FEB DAC module Voltage
   > dac set [nChannel] [nVolt]

  7) set&push FEB ASIC module raw configure
   > asic set raw [nChannel] [nData]

  8) get FEB HVOLT module Voltage
   > hvolt get

  9) set&push FEB HVOLT module Voltage
   > hvolt set [nVolt]
)"
 );


int main(int argc, char **argv){
  std::string c_opt;
  int c;
  while ( (c = getopt(argc, argv, "h")) != -1) {
    switch (c) {
    case 'h':
      fprintf(stdout, "%s", help_usage.c_str());
      return 0;
      break;
    default:
      fprintf(stderr, "%s", help_usage.c_str());
      return 1;
    }
  }


  off_t addrbase_phy = R_DAQB_PBASE;
  int fd = 0;
  pR_DAQB_VBASE32 = 0;
  map_mem_block(&fd, addrbase_phy, D_DAQB_PSIZE, &pR_DAQB_VBASE32);
  if(pR_DAQB_VBASE32==0){
    std::fprintf(stderr, "Memory mapped failed\n");
    ummap_mem_block(&fd, &pR_DAQB_VBASE32);
    exit(-1);
  }


  const char* linenoise_history_path = "/tmp/.linenoiseng_comet_wsf";
  linenoiseHistoryLoad(linenoise_history_path);
  linenoiseSetCompletionCallback([](const char* prefix, linenoiseCompletions* lc)
                                 {
                                   static const char* examples[] =
                                     {"help", "print", "init", "reset", "quit", "exit", "daqb", "feb", "set", "get", "cmd", "hvolt", "dac", "temp", "asic", "raw", "volt",
                                      NULL};
                                   size_t i;
                                   for (i = 0;  examples[i] != NULL; ++i) {
                                     if (strncmp(prefix, examples[i], strlen(prefix)) == 0) {
                                       linenoiseAddCompletion(lc, examples[i]);
                                     }
                                   }
                                 } );

  const char* prompt = "\x1b[1;32mcomet_wsf\x1b[0m> ";
  while (1) {
    char* result = linenoise(prompt);
    if (result == NULL) {
      break;
    }
    if ( std::regex_match(result, std::regex("\\s*(help)\\s*")) ){
      fprintf(stdout, "%s", help_usage_linenoise.c_str());
    }
    else if ( std::regex_match(result, std::regex("\\s*(quit|exit)\\s*")) ){
      printf("quiting \n");
      linenoiseHistoryAdd(result);
      free(result);
      break;
    }
    else if ( std::regex_match(result, std::regex("\\s*(reset)\\s*")) ){
      printf("reset TODO\n");
      //TODO
    }
    else if ( std::regex_match(result, std::regex("\\s*(init)\\s*")) ){
      printf("init TODO\n");
      //TODO
    }
    else if ( std::regex_match(result, std::regex("\\s*(daqb)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(daqb)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*"));
      uint64_t addr = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      uint64_t data = std::stoull(mt[7].str(), 0, mt[6].str().empty()?10:16);
      daqb_reg_write(addr, data);
    }
    else if ( std::regex_match(result, std::regex("\\s*(daqb)\\s+(get)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(daqb)\\s+(get)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*"));
      uint64_t addr = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      daqb_reg_read(addr);
    }
    else if ( std::regex_match(result, std::regex("\\s*(feb)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(feb)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*"));
      uint64_t addr = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      uint64_t data = std::stoull(mt[7].str(), 0, mt[6].str().empty()?10:16);
      feb_cmd_write(addr, data);
    }
    else if ( std::regex_match(result, std::regex("\\s*(temp)\\s+(get)\\s*")) ){
      feb_read_new_temp();
    }
    else if ( std::regex_match(result, std::regex("\\s*(hvolt)\\s+(get)\\s*")) ){
      feb_read_hv();
    }
    else if ( std::regex_match(result, std::regex("\\s*(dac)\\s+(set)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(dac)\\s+(set)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*"));
      uint64_t ch = std::stoull(mt[4].str(), 0, mt[3].str().empty()?10:16);
      uint64_t voltage = std::stoull(mt[6].str(), 0, mt[5].str().empty()?10:16);
      feb_set_dac_voltage(ch, voltage);
    }
    else if ( std::regex_match(result, std::regex("\\s*(asic)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(asic)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*"));
      uint64_t ch = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      uint64_t data = std::stoull(mt[7].str(), 0, mt[6].str().empty()?10:16);
      feb_set_asic_raw(ch, data);
    }
    else if ( std::regex_match(result, std::regex("\\s*(hvolt)\\s+(set)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(hvolt)\\s+(set)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*"));
      uint64_t voltage = std::stoull(mt[4].str(), 0, mt[3].str().empty()?10:16);
      std::printf("Warning: not yet finished, no checksum\n");
      feb_set_hv_voltage(voltage);
    }
    else{
      std::printf("Error, unknown command:    %s\n", result);
    }

    linenoiseHistoryAdd(result);
    free(result);
  }

  linenoiseHistorySave(linenoise_history_path);
  linenoiseHistoryFree();

  ummap_mem_block(&fd, &pR_DAQB_VBASE32);
  return 0;
}
