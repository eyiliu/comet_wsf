#include <unistd.h>
#include <vector>
#include <iostream>
#include <chrono>

#include "TcpServer.hh"
#include "TcpConnection.hh"

TcpServer::TcpServer(short int port, uint16_t deviceid){
  m_deviceid = deviceid;
  m_isActive = true;
  m_fut = std::async(std::launch::async, &TcpServer::threadClientMananger, this, port);
}

TcpServer::~TcpServer(){
  if(m_fut.valid()){
    m_isActive = false;
    m_fut.get();
  }
}

uint64_t TcpServer::threadClientMananger(short int port){
  m_isActive = true;
  int sockfd = TcpConnection::createServerSocket(port);
  if(sockfd<0){
    printf("unable to create server listenning socket\n");
    m_isActive = false;
  }
  std::unique_ptr<TcpConnection> conn;

  while(m_isActive){
    if(conn && !(*conn)){
      conn.reset();
    }
    if(!conn){
      auto new_conn = TcpConnection::waitForNewClient(sockfd, std::chrono::seconds(1),
                                                      reinterpret_cast<FunProcessMessage>(&TcpServer::perConnProcessMessage),
                                                      reinterpret_cast<FunSendDeamon>(&TcpServer::perConnSendDeamon), this);
      if(new_conn){
        conn = std::move(new_conn);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  if(sockfd>=0){
    TcpConnection::closeSocket(sockfd);
  }
  printf("server is closed\n");
  return 0;
}


int TcpServer::perConnProcessMessage(void* pconn, msgpack::object_handle &oh){
  msgpack::object msg = oh.get();
  unique_zone& life = oh.zone();
  NetMsg netmsg = msg.as<NetMsg>();
  switch(netmsg.type){
  case NetMsg::Type::data :{
    break;
  }
  case NetMsg::Type::daqinit :{
    std::cout<< "datatking init"<<std::endl;
    std::cout<< "do nothing"<<std::endl;
    break;
  }
  case NetMsg::Type::daqconf :{
    std::cout<< "datatking conf"<<std::endl;
    m_fw.daq_conf_default();
    break;
  }
  case NetMsg::Type::daqreset :{
    std::cout<< "datatking reset"<<std::endl;
    m_fw.daq_reset();
    break;
  }
  case NetMsg::Type::daqstart :{
    std::cout<< "datatking start"<<std::endl;
    m_fw.daq_start_run();
    break;
  }
  case NetMsg::Type::daqstop :{
    std::cout<< "datatking stop"<<std::endl;
    m_fw.daq_stop_run();
    break;
  }
  default:
    std::cout<< "unknown msg type"<<std::endl;
  }
  return 0;
}

int TcpServer::perConnSendDeamon(void  *pconn){
  TcpConnection* conn = reinterpret_cast<TcpConnection*>(pconn);
  std::stringstream ssbuf;
  std::string strbuf;
  while((*conn)){
    auto df_pack = m_rd.readPack(std::chrono::seconds(1));
    if(df_pack.empty()){
      std::fprintf(stdout, "Data reveving timeout\n");
      continue;
    }
    if(df_pack.size()!=8){
      std::fprintf(stderr, "Error Package size is not 8 bytes.\n");
      throw;
    }
    uint8_t headM  = 0;
    uint8_t wireN = 0;
    uint16_t hitN = 0;
    uint64_t timeV = 0;
    std::tie(headM, wireN, hitN, timeV) = comet_wsf::comet_datarecv::decode_pack(df_pack);

    std::fprintf(stdout, "RawData_RX: %s\n", comet_wsf::comet_datarecv::binToHexString(df_pack).c_str());
    std::fprintf(stdout, "Pack decode: [headM %hhu, WireN %hhu, HitN %hu, TimeV %llu] \n", headM, wireN, hitN, timeV);


    NetMsg dataMsg{NetMsg::data, 0, 0, 0, headM, wireN, hitN, timeV, {std::vector<char>(df_pack.begin(), df_pack.end())}};
    ssbuf.str(std::string());
    msgpack::pack(ssbuf, dataMsg);
    strbuf = ssbuf.str();

    conn->sendRaw(strbuf.data(), strbuf.size());
  }
  return 0;
}
