#pragma once

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <future>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "TcpConnection.hh"
#include "comet_wsf_regctrl.hh"
#include "comet_wsf_datarecv.hh"

class TcpServer{
public:
  TcpServer() = delete;
  TcpServer(const TcpServer&) =delete;
  TcpServer& operator=(const TcpServer&) =delete;
  TcpServer(short int port, uint16_t deviceid);
  ~TcpServer();

  uint64_t threadClientMananger(short int port);

  int perConnProcessMessage(void* pconn, msgpack::object_handle &oh);
  int perConnSendDeamon(void* pconn);

private:
  std::future<uint64_t> m_fut;
  bool m_isActive{false};
  comet_wsf::comet_regctrl  m_fw;
  comet_wsf::comet_datarecv m_rd;

  uint16_t m_deviceid;
};

