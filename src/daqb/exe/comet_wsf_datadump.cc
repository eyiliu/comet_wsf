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

#include "comet_wsf_system.hh"
#include "comet_wsf_datarecv.hh"

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

  std::unique_ptr<comet_wsf::comet_datarecv> recv;
  try{
    recv.reset(new comet_wsf::comet_datarecv);
  }catch(...){
    recv.reset();
    exit(-1);
  }

  std::chrono::system_clock::time_point tp_timeout_exit  = std::chrono::system_clock::now() + std::chrono::seconds(exitTimeSecond);

  std::future<uint64_t> fut_async_watch;
  fut_async_watch = std::async(std::launch::async, &AsyncWatchDog);
  while(!g_done){
    if(exitTimeSecond && std::chrono::system_clock::now() > tp_timeout_exit){
      std::fprintf(stdout, "run %d seconds, nornal exit\n", exitTimeSecond);
      break;
    }
    auto df_pack = recv->readPack(std::chrono::seconds(1));
    if(df_pack.empty()){
      std::fprintf(stdout, "Data reveving timeout\n");
      continue;
    }
    if(df_pack.size()!=8){
      std::fprintf(stderr, "Error Package size is not 8 bytes.\n");
      throw;
    }
    if(do_rawPrint ){
      std::fprintf(stdout, "RawData_RX:\n%s\n", comet_wsf::comet_datarecv::binToHexString(df_pack).c_str());
      uint8_t headM  = 0;
      uint8_t wireN = 0;
      uint16_t hitN = 0;
      uint64_t timeV = 0;
      std::tie(headM, wireN, hitN, timeV) = comet_wsf::comet_datarecv::decode_pack(df_pack);
      std::fprintf(stdout, "Pack decode: [headM %hhu, WireN %hhu, HitN %hu, TimeV %llu] \n", headM, wireN, hitN, timeV);
      std::fflush(stdout);
    }
    if(fp){
      std::fwrite(df_pack.data(), 1, df_pack.size(), fp);
      std::fflush(fp);
    }
  }

  if(fp){
    std::fflush(fp);
    std::fclose(fp);
  }

  g_done= 1;
  if(fut_async_watch.valid())
    fut_async_watch.get();

  return 0;
}
