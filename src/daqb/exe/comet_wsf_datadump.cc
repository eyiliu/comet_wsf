#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <future>
#include <thread>

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "getopt.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::size_t;


#ifndef DEBUG_PRINT
#define DEBUG_PRINT 0
#endif
#define debug_print(fmt, ...)                                           \
  do { if (DEBUG_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)


static sig_atomic_t g_done = 0;

std::atomic<size_t> ga_dataFrameN = 0;

uint64_t AsyncWatchDog(){
  auto tp_run_begin = std::chrono::system_clock::now();
  auto tp_old = tp_run_begin;
  size_t st_old_dataFrameN = 0;

  while(!g_done){
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto tp_now = std::chrono::system_clock::now();
    std::chrono::duration<double> dur_period_sec = tp_now - tp_old;
    std::chrono::duration<double> dur_accu_sec = tp_now - tp_run_begin;
    double sec_period = dur_period_sec.count();
    double sec_accu = dur_accu_sec.count();
    size_t st_dataFrameN = ga_dataFrameN;
    // double st_hz_pack_accu = st_dataFrameN / sec_accu;
    // double st_hz_pack_period = (st_dataFrameN-st_old_dataFrameN) / sec_period;

    tp_old = tp_now;
    st_old_dataFrameN= st_dataFrameN;
    // std::fprintf(stdout, "                  ev_accu(%8.2f hz) ev_trans(%8.2f hz) last_id(%8.2hu)\r",st_hz_pack_accu, st_hz_pack_period, st_lastTriggerId);
    std::fflush(stdout);
  }
  std::fprintf(stdout, "\n\n");
  return 0;
}

namespace{
  std::string binToHexString(const char *bin, int len){
    constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                               '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    const unsigned char* data = (const unsigned char*)(bin);
    std::string s(len * 2, ' ');
    for (int i = 0; i < len; ++i) {
      s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
      s[2 * i + 1] = hexmap[data[i] & 0x0F];
    }
    return s;
  }

  std::string hexToBinString(const char *hex, int len){
    if(len%2){
      throw;
    }
    size_t blen  = len/2;
    const unsigned char* data = (const unsigned char*)(hex);
    std::string s(blen, ' ');
    for (int i = 0; i < blen; ++i){
      unsigned char d0 = data[2*i];
      unsigned char d1 = data[2*i+1];
      unsigned char v0;
      unsigned char v1;
      if(d0>='0' && d0<='9'){
        v0 = d0-'0';
      }
      else if(d0>='a' && d0<='f'){
        v0 = d0-'a'+10;
      }
      else if(d0>='A' && d0<='F'){
        v0 = d0-'A'+10;
      }
      else{
        std::fprintf(stderr, "wrong hex string\n");
        throw;
      }
      if(d1>='0' && d1<='9'){
        v1 = d1-'0';
      }
      else if(d1>='a' && d1<='f'){
        v1 = d1-'a'+10;
      }
      else if(d1>='A' && d1<='F'){
        v1 = d1-'A'+10;
      }
      else{
        std::fprintf(stderr, "wrong hex string\n");
        throw;
      }
      s[i]= (v0<<4) + v1;
    }
    return s;
  }

  std::string binToHexString(const std::string& bin){
    return binToHexString(bin.data(), bin.size());
  }

  std::string hexToBinString(const std::string& hex){
    return hexToBinString(hex.data(), hex.size());
  }

  std::string readPack(int fd_rx, const std::chrono::milliseconds &timeout_idel){ //timeout_read_interval
    size_t size_buf = 8;
    std::string buf(size_buf, 0);
    size_t size_filled = 0;
    std::chrono::system_clock::time_point tp_timeout_idel;
    bool can_time_out = false;
    int read_len_real = 0;
    while(size_filled < size_buf){
      read_len_real = read(fd_rx, &buf[size_filled], size_buf-size_filled);
      // if(read_len_real>0){//for debugging print 
      //   std::string str_new(&buf[size_filled],  read_len_real);
      //   std::fprintf(stdout, "RawData_RX new buffer: %s\n", binToHexString(str_new).c_str());
      // }
      if(read_len_real>0){
        size_filled += read_len_real;
        can_time_out = false;
      }
      else if (read_len_real== 0 || (read_len_real < 0 && errno == EAGAIN)){ // empty readback, read again
        if(!can_time_out){
          can_time_out = true;
          tp_timeout_idel = std::chrono::system_clock::now() + timeout_idel;
        }
        else{ // can_time_out == true;
          if(std::chrono::system_clock::now() > tp_timeout_idel){
            if(size_filled == 0){
              debug_print("INFO<%s>: no data receving.\n",  __func__);
              return std::string();
            }
            std::fprintf(stderr, "ERROR<%s>: timeout error of incomplete data reading \n", __func__ );
            std::fprintf(stderr, "=");
            return std::string();
          }
        }
        continue;
      }
      else{
        std::fprintf(stderr, "ERROR<%s>: read(...) returns error code %d\n", __func__,  errno);
        throw;
      }
    }
    // std::fprintf(stdout, "dumpping data Hex:\n%s\n", StringToHexString(buf).c_str());
    return buf;
  }

}




static const std::string help_usage = R"(
Usage:
  -help                        help message
  -verbose                     verbose flag
  -rawPrint                    print data by hex format in terminal
  -decode                      decode the raw data
  -rawFile        <path>       path of raw file to save
  -exitTime       <n>          exit after n seconds (0=NoLimit, default 10)
examples:
#1. save data and print
comet_wsf_datadump -rawPrint -rawFile test.dat

#2. save data only
comet_wsf_datadump -rawFile test.dat

#3. print only
comet_wsf_datadump -rawPrint

#4. print, exit after 60 seconds
comet_wsf_datadump -rawPrint -exitTime 60

)";

int main(int argc, char *argv[]) {
  signal(SIGINT, [](int){g_done+=1;});

  std::string rawFilePath;
  std::string ipAddressStr;
  int exitTimeSecond = 10;
  bool do_rawPrint = false;
  bool do_decode = false;
  int do_verbose = 0;
  {////////////getopt begin//////////////////
    struct option longopts[] = {{"help",      no_argument, NULL, 'h'},//option -W is reserved by getopt
                                {"verbose",   no_argument, NULL, 'v'},//val
                                {"decode",    no_argument, NULL, 'd'},
                                {"rawPrint",  no_argument, NULL, 's'},
                                {"rawFile",   required_argument, NULL, 'f'},
                                {"exitTime",  required_argument, NULL, 'e'},
                                {0, 0, 0, 0}};

    if(argc == 1){
      std::fprintf(stderr, "%s\n", help_usage.c_str());
      std::exit(1);
    }
    int c;
    int longindex;
    opterr = 1;
    while ((c = getopt_long_only(argc, argv, "-", longopts, &longindex)) != -1) {
      // // "-" prevents non-option argv
      // if(!optopt && c!=0 && c!=1 && c!=':' && c!='?'){ //for debug
      //   std::fprintf(stdout, "opt:%s,\targ:%s\n", longopts[longindex].name, optarg);;
      // }
      switch (c) {
      case 'f':
        rawFilePath = optarg;
        break;
      case 'e':
        exitTimeSecond = std::stoi(optarg);
        break;
      case 's':
        do_rawPrint = true;
        break;
      case 'd':
        do_decode = true;
        break;
        // help and verbose
      case 'v':
        do_verbose=1;
        //option is set to no_argument
        if(optind < argc && *argv[optind] != '-'){
          do_verbose = std::stoul(argv[optind]);
          optind++;
        }
        break;
      case 'h':
        std::fprintf(stdout, "%s\n", help_usage.c_str());
        std::exit(0);
        break;
        /////generic part below///////////
      case 0:
        // getopt returns 0 for not-NULL flag option, just keep going
        break;
      case 1:
        // If the first character of optstring is '-', then each nonoption
        // argv-element is handled as if it were the argument of an option
        // with character code 1.
        std::fprintf(stderr, "%s: unexpected non-option argument %s\n",
                     argv[0], optarg);
        std::exit(1);
        break;
      case ':':
        // If getopt() encounters an option with a missing argument, then
        // the return value depends on the first character in optstring:
        // if it is ':', then ':' is returned; otherwise '?' is returned.
        std::fprintf(stderr, "%s: missing argument for option %s\n",
                     argv[0], longopts[longindex].name);
        std::exit(1);
        break;
      case '?':
        // Internal error message is set to print when opterr is nonzero (default)
        std::exit(1);
        break;
      default:
        std::fprintf(stderr, "%s: missing getopt branch %c for option %s\n",
                     argv[0], c, longopts[longindex].name);
        std::exit(1);
        break;
      }
    }
  }/////////getopt end////////////////

  std::fprintf(stdout, "\n");
  std::fprintf(stdout, "rawPrint:  %d\n", do_rawPrint);
  std::fprintf(stdout, "rawFile:   %s\n", rawFilePath.c_str());
  std::fprintf(stdout, "\n");

  std::FILE *fp = nullptr;
  if(!rawFilePath.empty()){
    std::filesystem::path filepath(rawFilePath);
    std::filesystem::path path_dir_output = std::filesystem::absolute(filepath).parent_path();
    std::filesystem::file_status st_dir_output =
      std::filesystem::status(path_dir_output);
    if (!std::filesystem::exists(st_dir_output)) {
      std::fprintf(stdout, "Output folder does not exist: %s\n\n",
                   path_dir_output.c_str());
      std::filesystem::file_status st_parent =
        std::filesystem::status(path_dir_output.parent_path());
      if (std::filesystem::exists(st_parent) &&
          std::filesystem::is_directory(st_parent)) {
        if (std::filesystem::create_directory(path_dir_output)) {
          std::fprintf(stdout, "Create output folder: %s\n\n", path_dir_output.c_str());
        } else {
          std::fprintf(stderr, "Unable to create folder: %s\n\n", path_dir_output.c_str());
          throw;
        }
      } else {
        std::fprintf(stderr, "Unable to create folder: %s\n\n", path_dir_output.c_str());
        throw;
      }
    }

    std::filesystem::file_status st_file = std::filesystem::status(filepath);
    if (std::filesystem::exists(st_file)) {
      std::fprintf(stderr, "File < %s > exists.\n\n", filepath.c_str());
      throw;
    }

    fp = std::fopen(filepath.c_str(), "w");
    if (!fp) {
      std::fprintf(stderr, "File opening failed: %s \n\n", filepath.c_str());
      throw;
    }
  }

  std::fprintf(stdout, " connecting to %s\n", "/dev/axidmard");
  int fd_rx = open("/dev/axidmard", O_RDONLY | O_NONBLOCK);
  if(!fd_rx){
    std::fprintf(stdout, " connection fail\n");
    throw;
  }
  std::fprintf(stdout, " connected\n");

  std::chrono::system_clock::time_point tp_timeout_exit  = std::chrono::system_clock::now() + std::chrono::seconds(exitTimeSecond);

  std::future<uint64_t> fut_async_watch;
  fut_async_watch = std::async(std::launch::async, &AsyncWatchDog);
  while(!g_done){
    if(exitTimeSecond && std::chrono::system_clock::now() > tp_timeout_exit){
      std::fprintf(stdout, "run %d seconds, nornal exit\n", exitTimeSecond);
      break;
    }
    auto df_pack = readPack(fd_rx, std::chrono::seconds(1));
    // auto df = rd->Read(std::chrono::seconds(1));
    if(df_pack.empty()){
      std::fprintf(stdout, "Data reveving timeout\n");
      continue;
    }
    if(df_pack.size()!=8){
      std::fprintf(stderr, "Error Package size is not 8 bytes.\n");
      throw;
    }
    if(do_rawPrint ){
      std::fprintf(stdout, "RawData_RX:\n%s\n", binToHexString(df_pack).c_str());
      const uint8_t* ppack = reinterpret_cast<const uint8_t *>(df_pack.data());
      uint8_t wireN = ( ((*ppack)&0x0f) << 4) | ( ((*(ppack+1)) & 0xf0)>>4 );
      uint16_t hitN  = ( ((uint16_t)(*(ppack+1) & 0x0f)) << 12) |
        (((uint16_t)(*(ppack+2)))<<4) | (((uint16_t)(*(ppack+3)) & 0xf0 )>>4);
      uint64_t timeV  = ( ((uint64_t)(*(ppack+3) & 0x0f)) << 32) |
        (((uint32_t)(*(ppack+4)))<<24) | (((uint32_t)(*(ppack+5)))<<16) |
        (((uint32_t)(*(ppack+6)))<<8) |   ((uint32_t)(*(ppack+7)));

      std::fprintf(stdout, "Pack decode: [WireN %hhu, HitN %hu, TimeV %llu] \n", wireN, hitN, timeV);
      std::fflush(stdout);
    }
    if(fp){
      std::fwrite(df_pack.data(), 1, df_pack.size(), fp);
      std::fflush(fp);
    }
  }

  close(fd_rx);
  if(fp){
    std::fflush(fp);
    std::fclose(fp);
  }

  g_done= 1;
  if(fut_async_watch.valid())
    fut_async_watch.get();

  return 0;
}

