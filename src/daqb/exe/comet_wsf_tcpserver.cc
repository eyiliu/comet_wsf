#include "TcpServer.hh"
#include <iostream>
#include <csignal>
static sig_atomic_t g_done = 0;


int main(int argn, char ** argc){
  int n = 0;
  if(argn != 1){
    n = std::stoi(std::string(argc[1]));
  }

  signal(SIGINT,[](int){g_done+=1;});
  std::unique_ptr<TcpServer> tcpServer(new TcpServer(9000, n));
  while(!g_done){
    std::this_thread::sleep_for (std::chrono::seconds(1));
  }
  return 0;
}
