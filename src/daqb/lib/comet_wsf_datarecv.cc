
#include "comet_wsf_datarecv.hh"


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


namespace comet_wsf{

  std::string comet_datarecv::binToHexString(const char *bin, int len){
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

  std::string comet_datarecv::hexToBinString(const char *hex, int len){
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

  std::string comet_datarecv::binToHexString(const std::string& bin){
    return binToHexString(bin.data(), bin.size());
  }

  std::string comet_datarecv::hexToBinString(const std::string& hex){
    return hexToBinString(hex.data(), hex.size());
  }

  std::string comet_datarecv::readPack(int fd_rx, const std::chrono::milliseconds &timeout_idel){ //timeout_read_interval
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

  comet_datarecv::comet_datarecv(){
    std::fprintf(stdout, " connecting to %s\n", "/dev/axidmard");
    fd_rx = open("/dev/axidmard", O_RDONLY | O_NONBLOCK);
    if(!fd_rx){
      std::fprintf(stdout, " connection fail\n");
      throw;
    }
    std::fprintf(stdout, " connected\n");
  }

  comet_datarecv::~comet_datarecv(){
    close(fd_rx);
  }

  std::string comet_datarecv::readPack(const std::chrono::milliseconds &timeout_idel){
    return readPack(fd_rx, timeout_idel);
  }

  std::tuple<uint8_t, uint8_t, uint16_t, uint64_t> comet_datarecv::decode_pack(std::string &raw){
    uint8_t headM  = 0;
    uint8_t wireN = 0;
    uint16_t hitN = 0;
    uint64_t timeV = 0;

    if(raw.size()!=8){
      headM = 0xff;
      //something wrong.
    }
    else{
      const uint8_t* ppack = reinterpret_cast<const uint8_t *>(raw.data());
      headM = ((*ppack) & 0xf0)>>4 ;
      wireN = ( ((*ppack)&0x0f) << 4) | ( ((*(ppack+1)) & 0xf0)>>4 );
      hitN  = ( ((uint16_t)(*(ppack+1) & 0x0f)) << 12) |
        (((uint16_t)(*(ppack+2)))<<4) | (((uint16_t)(*(ppack+3)) & 0xf0 )>>4);
      timeV  = ( ((uint64_t)(*(ppack+3) & 0x0f)) << 32) |
        (((uint32_t)(*(ppack+4)))<<24) | (((uint32_t)(*(ppack+5)))<<16) |
        (((uint32_t)(*(ppack+6)))<<8) |   ((uint32_t)(*(ppack+7)));
    }
    return std::make_tuple(headM, wireN, hitN, timeV);
  }
}
