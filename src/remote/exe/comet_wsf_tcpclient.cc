#include <cstdio>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <ctime>

#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <future>
#include <thread>
#include <regex>


#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <signal.h>
#include <arpa/inet.h>

#include "linenoise.h"
#include "getopt.h"

#include "daqb.hh"

static  const std::string help_usage
(R"(
Usage:
-h : print usage information, and then quit
-dataFolder        <pathFolder> path of data folder to save
-daqbHost          <ipAddress>  ip address (hostname) of DAQB server
-daqbPort          <numPort>    port of DAQB server


'help' command in interactive mode provides detail usage information
)"
 );


static  const std::string help_usage_linenoise
(R"(

keyword: help, print, reset, conf, start, stop, quit, daqb, feb, set, get, hvolt, dac, temp, asic, raw
example:
  1) quit command line
   > quit

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


std::string TimeNowString(const std::string& format){
    std::time_t time_now = std::time(nullptr);
    std::string str_buffer(100, char(0));
    size_t n = std::strftime(&str_buffer[0], sizeof(str_buffer.size()),
                             format.c_str(), std::localtime(&time_now));
    str_buffer.resize(n?(n-1):0);
    return str_buffer;
}

bool check_and_create_folder(std::filesystem::path path_dir){

  std::filesystem::path path_dir_output = std::filesystem::absolute(path_dir);
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
  return true;
}


std::FILE * create_and_open_file(std::filesystem::path filepath){

  std::filesystem::path filepath_abs = std::filesystem::absolute(filepath);
  std::filesystem::file_status st_file = std::filesystem::status(filepath_abs);
  if (std::filesystem::exists(st_file)) {
    std::fprintf(stderr, "File < %s > exists.\n\n", filepath_abs.c_str());
    throw;
  }

  std::FILE *fp = std::fopen(filepath_abs.c_str(), "w");
  if (!fp) {
    std::fprintf(stderr, "File opening failed: %s \n\n", filepath_abs.c_str());
    throw;
  }
  return fp;
}


static sig_atomic_t g_data_done = 0;
static sig_atomic_t g_watch_done = 0;

uint64_t AsyncWatchDog();
uint64_t AsyncDataSave(std::FILE *p_fd, comet_wsf::daqb *p_daqb);

int main(int argc, char **argv){
  std::string dataFolderPath;
  std::string daqbHost_ipstr;
  uint16_t daqbPortN = 9000;
  int do_verbose = 0;
  {////////////getopt begin//////////////////
    struct option longopts[] = {{"help",      no_argument, NULL, 'h'},//option -W is reserved by getopt
                                {"verbose",   no_argument, NULL, 'v'},//val
                                {"dataFolder",   required_argument, NULL, 'd'},
                                {"daqbHost",  required_argument, NULL, 'o'},
                                {"daqbPort",  required_argument, NULL, 'r'},
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
      case 'd':
        dataFolderPath = optarg;
        break;
      case 'o':
        daqbHost_ipstr = optarg;
      case 'r':
        daqbPortN  = atoi(optarg);
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
  std::fprintf(stdout, "dataFolder:   %s\n", dataFolderPath.c_str());
  std::fprintf(stdout, "\n");

  if(dataFolderPath.empty()){
      std::fprintf(stdout, "option dataFolder is missing\n");
      std::exit(1);
  }

  std::filesystem::path data_folder_path(dataFolderPath);
  std::filesystem::path data_folder_path_abs = std::filesystem::absolute(data_folder_path);
  check_and_create_folder(data_folder_path_abs);

  std::FILE *fp_data=0;

  std::unique_ptr<comet_wsf::daqb> daqb;
  try{
    daqb.reset(new comet_wsf::daqb("comet_wsf_daqb", daqbHost_ipstr, daqbPortN));
  }catch(...){
    daqb.reset();
    exit(-1);
  }

  const char* linenoise_history_path = "/tmp/.linenoiseng_comet_wsf_client";
  linenoiseHistoryLoad(linenoise_history_path);
  linenoiseSetCompletionCallback([](const char* prefix, linenoiseCompletions* lc)
                                 {
                                   static const char* examples[] =
                                     {"help", "print", "reset", "conf", "start", "stop", "quit", "exit",
                                      NULL};
                                   size_t i;
                                   for (i = 0;  examples[i] != NULL; ++i) {
                                     if (strncmp(prefix, examples[i], strlen(prefix)) == 0) {
                                       linenoiseAddCompletion(lc, examples[i]);
                                     }
                                   }
                                 } );


  std::future<uint64_t> fut_async_watch;
  std::future<uint64_t> fut_async_data;

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
      daqb->daq_reset();
      g_data_done = 1;
      if(fut_async_data.valid())
        fut_async_data.get();
      //daqb.reset(new comet_wsf::daqb("comet_wsf_daqb", daqbHost_ipstr, daqbPortN));
    }
    else if ( std::regex_match(result, std::regex("\\s*(conf)\\s*")) ){
      printf("conf\n");
      daqb->daq_conf_default();
    }
    else if ( std::regex_match(result, std::regex("\\s*(start)\\s*")) ){
      printf("start\n");
      std::string name_datafile = std::string("comet_data_")+TimeNowString("%y%m%d%H%M%S")+std::string(".dat");
      std::filesystem::path file_data_path = data_folder_path_abs/name_datafile;
      fp_data = create_and_open_file(file_data_path);
      fut_async_data = std::async(std::launch::async, &AsyncDataSave, fp_data, daqb.get());
      daqb->daq_start_run();
    }
    else if ( std::regex_match(result, std::regex("\\s*(stop)\\s*")) ){
      printf("stop\n");
      daqb->daq_stop_run();
      g_data_done = 1;
      if(fut_async_data.valid())
        fut_async_data.get();
    }
    else{
      std::printf("Error, unknown command:    %s\n", result);
    }

    linenoiseHistoryAdd(result);
    free(result);
  }

  linenoiseHistorySave(linenoise_history_path);
  linenoiseHistoryFree();

  g_data_done = 1;
  if(fut_async_data.valid())
    fut_async_data.get();

  g_watch_done = 1;
  if(fut_async_watch.valid())
    fut_async_watch.get();
  return 0;
}

uint64_t AsyncWatchDog(){
  auto tp_run_begin = std::chrono::system_clock::now();
  auto tp_old = tp_run_begin;
  size_t st_old_dataFrameN = 0;

  while(!g_watch_done){
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto tp_now = std::chrono::system_clock::now();
    std::chrono::duration<double> dur_period_sec = tp_now - tp_old;
    std::chrono::duration<double> dur_accu_sec = tp_now - tp_run_begin;
    double sec_period = dur_period_sec.count();
    double sec_accu = dur_accu_sec.count();
    // size_t st_dataFrameN = ga_dataFrameN;
    // double st_hz_pack_accu = st_dataFrameN / sec_accu;
    // double st_hz_pack_period = (st_dataFrameN-st_old_dataFrameN) / sec_period;

    tp_old = tp_now;
    // st_old_dataFrameN= st_dataFrameN;
    // std::fprintf(stdout, "                  ev_accu(%8.2f hz) ev_trans(%8.2f hz) last_id(%8.2hu)\r",st_hz_pack_accu, st_hz_pack_period, st_lastTriggerId);
    // std::fflush(stdout);
  }
  // std::fprintf(stdout, "\n\n");
  return 0;
}


uint64_t AsyncDataSave(std::FILE *p_fd, comet_wsf::daqb *p_daqb){

  while(!g_watch_done){
    auto pack = p_daqb->Front();
    if(!pack){
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    p_daqb->PopFront();
    uint16_t headM  = pack->headM;
    uint16_t wireN = pack->wireN;
    uint16_t hitN = pack->hitN;
    uint64_t timeV = pack->timeV;

    std::fprintf(p_fd, "%hu  %hu  %llu \n", wireN, hitN, timeV);
    std::fflush(p_fd);
  }

  std::fclose(p_fd);
  return 0;
}
