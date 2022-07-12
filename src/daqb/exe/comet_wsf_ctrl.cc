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

#include "comet_wsf_regctrl.hh"
#include "comet_wsf_regdefine.hh"

static  const std::string help_usage
(R"(
Usage:
-h : print usage information, and then quit

'help' command in interactive mode provides detail usage information
)"
 );


static  const std::string help_usage_linenoise
(R"(

keyword: help, print, reset, conf, start, stop, quit, daqb, feb, set, get, hvolt, dac, temp, asic, raw
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

  10) reset FEB and DAQB
   > reset

  11) set&push default configure to FEB (DAC, HVOLT)
   > conf

  12) start data taking
   > start

  13) stop data taking
   > stop
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

  std::unique_ptr<comet_wsf::comet_regctrl> regctrl;
  try{
    regctrl.reset(new comet_wsf::comet_regctrl);
  }catch(...){
    regctrl.reset();
    exit(-1);
  }

  const char* linenoise_history_path = "/tmp/.linenoiseng_comet_wsf";
  linenoiseHistoryLoad(linenoise_history_path);
  linenoiseSetCompletionCallback([](const char* prefix, linenoiseCompletions* lc)
                                 {
                                   static const char* examples[] =
                                     {"help", "print", "reset", "conf", "start", "stop", "quit", "exit", "daqb", "feb", "set", "get", "hvolt", "dac", "temp", "asic", "raw",
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
      printf("reset\n");
      regctrl->daq_reset();
    }
    else if ( std::regex_match(result, std::regex("\\s*(conf)\\s*")) ){
      printf("conf\n");
      regctrl->daq_conf_default();
    }
    else if ( std::regex_match(result, std::regex("\\s*(start)\\s*")) ){
      printf("start\n");
      regctrl->daq_start_run();
    }
    else if ( std::regex_match(result, std::regex("\\s*(stop)\\s*")) ){
      printf("stop\n");
      regctrl->daq_stop_run();
    }
    else if ( std::regex_match(result, std::regex("\\s*(daqb)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(daqb)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*"));
      uint64_t addr = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      uint64_t data = std::stoull(mt[7].str(), 0, mt[6].str().empty()?10:16);
      regctrl->daqb_reg_write(addr, data);
    }
    else if ( std::regex_match(result, std::regex("\\s*(daqb)\\s+(get)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(daqb)\\s+(get)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*"));
      uint64_t addr = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      regctrl->daqb_reg_read(addr);
    }
    else if ( std::regex_match(result, std::regex("\\s*(feb)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(feb)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*"));
      uint64_t addr = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      uint64_t data = std::stoull(mt[7].str(), 0, mt[6].str().empty()?10:16);
      regctrl->feb_cmd_write(addr, data);
    }
    else if ( std::regex_match(result, std::regex("\\s*(temp)\\s+(get)\\s*")) ){
      regctrl->feb_read_new_temp();
    }
    else if ( std::regex_match(result, std::regex("\\s*(hvolt)\\s+(get)\\s*")) ){
      regctrl->feb_read_hv();
    }
    else if ( std::regex_match(result, std::regex("\\s*(dac)\\s+(set)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+([0-9\\.]+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(dac)\\s+(set)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+([0-9\\.]+)\\s*"));
      uint64_t ch = std::stoull(mt[4].str(), 0, mt[3].str().empty()?10:16);
      double voltage = std::stod(mt[5].str());
      regctrl->feb_set_dac_voltage(ch, voltage);
    }
    else if ( std::regex_match(result, std::regex("\\s*(asic)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(asic)\\s+(set)\\s+(raw)\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s+(?:(0[Xx])?([0-9a-fA-F]+))\\s*"));
      uint64_t ch = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      uint64_t data = std::stoull(mt[7].str(), 0, mt[6].str().empty()?10:16);
      regctrl->feb_set_asic_raw(ch, data);
    }
    else if ( std::regex_match(result, std::regex("\\s*(hvolt)\\s+(set)\\s+([0-9\\.]+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(hvolt)\\s+(set)\\s+([0-9\\.]+)\\s*"));
      double voltage = std::stod(mt[3].str());
      std::printf("Warning: not yet finished, no checksum\n");
      regctrl->feb_set_hv_voltage(voltage);
    }
    else{
      std::printf("Error, unknown command:    %s\n", result);
    }

    linenoiseHistoryAdd(result);
    free(result);
  }

  linenoiseHistorySave(linenoise_history_path);
  linenoiseHistoryFree();

  return 0;
}
