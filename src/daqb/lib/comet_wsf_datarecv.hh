#pragma once

#include "comet_wsf_system.hh"

#include <tuple>

namespace comet_wsf{
  class comet_datarecv{
  public:
    comet_datarecv();
    ~comet_datarecv();

    std::string readPack(const std::chrono::milliseconds &timeout_idel);
  private:
    int fd_rx{0};
  public:
    static std::string readPack(int fd_rx, const std::chrono::milliseconds &timeout_idel);
    static std::string hexToBinString(const std::string& hex);
    static std::string binToHexString(const std::string& bin);
    static std::string hexToBinString(const char *hex, int len);
    static std::string binToHexString(const char *bin, int len);

    static std::tuple<uint8_t, uint8_t, uint16_t, uint64_t> decode_pack(std::string &raw);

  };

}
