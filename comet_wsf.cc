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


#include "linenoise.h"


using std::uint32_t;
using std::size_t;
using namespace std::chrono_literals;


void map_mem_block(int* p_fd, uint32_t paddr, uint32_t psize, uint32_t** pp_vbase32){
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




#define R_DAQB_PBASE    0x43c00000
#define D_DAQB_PSIZE    0x100

#define R_DAQB_CMD_HADDR_LDATA  0x0
#define R_DAQB_DATA_RESET       0x4
#define R_DAQB_DATA_RUN         0x8
#define R_DAQB_CMD_ACK          0xc
#define R_DAQB_TEMP             0x10
#define R_DAQB_HVOLT            0x14

#define U_TEMP_C_PER            0.0078125
#define U_DAC_V_PER             0.0008059

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
  *(vbase+o) = d;
}


uint32_t daqb_reg_read(uint32_t o, uint32_t* vbase){
  uint32_t data =  *(vbase+o);
  if(o!=R_DAQB_CMD_ACK){
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
  uint32_t cmd = a<<16 + d;
  std::printf("writing DAQB register:  %#010x @ %#010x \n", d, a);
  daqb_reg_write(R_DAQB_CMD_HADDR_LDATA, cmd);
}


uint32_t daqb_read_last_ack_cnt(){
  return daqb_reg_read(R_DAQB_CMD_ACK);
}

void daqb_wait_for_ack_cnt(uint32_t last_cnt){
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

void feb_read_new_temp(){
  uint32_t last_temp = daqb_reg_read(R_DAQB_TEMP);
  uint32_t last_cmd_ack_cnt = daqb_read_last_ack_cnt();
  feb_cmd_write(R_FEB_TEMP_FETCH, 1);
  daqb_wait_for_ack_cnt(last_cmd_ack_cnt);
  uint32_t temp_new = daqb_reg_read(R_DAQB_TEMP);
  std::printf("temp raw new %#010x, last %#010x \n", temp_new, last_temp);
  std::printf("temp     new %u, last %u \n", temp_new*U_TEMP_C_PER, last_temp*U_TEMP_C_PER);
}


void feb_set_hv_voltage(){
  // TODO
}

void feb_set_hv_raw(){
  // TODO
}

void feb_read_hv_raw(){
  uint32_t hvolt_raw = R_DAQB_HVOLT;
  std::printf("hvolt raw %#010x \n", hvolt_raw);
}

void feb_set_dac_voltage(uint32_t ch, double v){
  uint32_t v_raw = lrint(v/U_DAC_V_PER);
  uint32_t data_raw = (ch & 0xff) << 12 + (v_raw & 0xffffff);
  feb_cmd_write(R_FEB_DAC_CONF, data_raw);
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

keyword: help, print, init, reset, quit, daqb, feb, set, get, cmd, hvolt, dac, temp, asic0, asic1, raw, volt
example:
  1) quit command line
   > quit

  2) get daqb regiester
   > daqb get raw [nAddress]

  3) set daqb regiester
   > daqb set raw [nAddress] [nData]

  4) set FEB register
   > feb set raw [nAddress] [nData]

  5) get temp module Celsius
   > temp get

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
  int fd;
  pR_DAQB_VBASE32 = 0;
  map_mem_block(&fd, addrbase_phy, D_DAQB_PSIZE, &pR_DAQB_VBASE32);
  if(fd==0 || pR_DAQB_VBASE32==0){
    std::fprintf(stderr, "Memory mapped failed\n");
    close(fd);
    exit(-1);
  }


  const char* linenoise_history_path = "/tmp/.linenoiseng_comet_wsf";
  linenoiseHistoryLoad(linenoise_history_path);
  linenoiseSetCompletionCallback([](const char* prefix, linenoiseCompletions* lc)
                                 {
                                   static const char* examples[] =
                                     {"help", "print", "init", "reset", "quit", "daqb", "feb", "set", "get", "cmd", "hvolt", "dac", "temp", "asic0", "asic1", "raw", "volt",
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
    else if ( std::regex_match(result, std::regex("\\s*(quit)\\s*")) ){
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

    else if ( std::regex_match(result, std::regex("\\s*(daqb)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9]+))\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(daqb)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9]+))\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      uint64_t addr = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      uint64_t data = std::stoull(mt[7].str(), 0, mt[6].str().empty()?10:16);
      daqb_reg_write(addr, data);
    }
    else if ( std::regex_match(result, std::regex("\\s*(daqb)\\s+(get)\\s+(raw)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(daqb)\\s+(get)\\s+(raw)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      uint64_t addr = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      daqb_reg_read(addr);
    }

    else if ( std::regex_match(result, std::regex("\\s*(feb)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9]+))\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(feb)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9]+))\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      uint64_t addr = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      uint64_t data = std::stoull(mt[7].str(), 0, mt[6].str().empty()?10:16);
      feb_cmd_write(addr, data);
    }
    else if ( std::regex_match(result, std::regex("\\s*(temp)\\s+(get)\\s*")) ){
      feb_read_new_temp();
    }


    linenoiseHistoryAdd(result);
    free(result);
  }

  linenoiseHistorySave(linenoise_history_path);
  linenoiseHistoryFree();

  close(fd);
  return 0;
}
