
#include <regex>
#include <iostream>
#include <thread>

#include "daqb.hh"

#ifndef DEBUG_PRINT
#define DEBUG_PRINT 0
#endif
#define debug_print(fmt, ...)                                           \
  do { if (DEBUG_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)

#ifndef INFO_PRINT
#define INFO_PRINT 0
#endif
#define info_print(fmt, ...)                                           \
  do { if (INFO_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)

using namespace comet_wsf;

daqb::~daqb(){
  m_conn.reset();

  m_is_async_watching = false;
  if(m_fut_async_watch.valid())
    m_fut_async_watch.get();
}

void daqb::daq_start_run(){
  m_vec_ring_ev.clear();
  m_vec_ring_ev.resize(m_size_ring);
  m_count_ring_write = 0;
  m_count_ring_read = 0;
  m_hot_p_read = m_size_ring -1; // tail

  m_flag_wait_first_event = true;

  m_st_n_tg_ev_now =0;
  m_st_n_ev_input_now =0;
  m_st_n_ev_output_now =0;
  m_st_n_ev_bad_now =0;
  m_st_n_ev_overflow_now =0;
  m_st_n_tg_ev_begin = 0;

  std::stringstream ssbuf;
  std::string strbuf;
  NetMsg daqMsg{NetMsg::daqstart, 0, 0, 0, 0, 0, 0, 0, {}};
  ssbuf.str(std::string());
  msgpack::pack(ssbuf, daqMsg);
  strbuf = ssbuf.str();
  m_isDataAccept= true;
  m_conn->sendRaw(strbuf.data(), strbuf.size());

  if(!m_is_async_watching){
    m_fut_async_watch = std::async(std::launch::async, &daqb::AsyncWatchDog, this);
  }
}

void daqb::daq_stop_run(){
  m_isDataAccept= false;

  std::stringstream ssbuf;
  std::string strbuf;
  NetMsg daqMsg{NetMsg::daqstop, 0, 0, 0, 0, 0, 0, 0, {}};
  ssbuf.str(std::string());
  msgpack::pack(ssbuf, daqMsg);
  strbuf = ssbuf.str();
  m_conn->sendRaw(strbuf.data(), strbuf.size());

  m_is_async_watching = false;
  if(m_fut_async_watch.valid())
    m_fut_async_watch.get();
}

void daqb::daq_reset(){
  m_isDataAccept= false;
  m_conn =  TcpConnection::connectToServer(m_host,  m_port, reinterpret_cast<FunProcessMessage>(&daqb::perConnProcessRecvMesg), nullptr, this);

  std::stringstream ssbuf;
  std::string strbuf;
  NetMsg daqMsg{NetMsg::daqreset, 0, 0, 0, 0, 0, 0, 0, {}};
  ssbuf.str(std::string());
  msgpack::pack(ssbuf, daqMsg);
  strbuf = ssbuf.str();
  m_conn->sendRaw(strbuf.data(), strbuf.size());
}


void daqb::daq_conf_default(){
  std::stringstream ssbuf;
  std::string strbuf;
  NetMsg daqMsg{NetMsg::daqconf, 0, 0, 0, 0, 0, 0, 0, {}};
  ssbuf.str(std::string());
  msgpack::pack(ssbuf, daqMsg);
  strbuf = ssbuf.str();
  m_conn->sendRaw(strbuf.data(), strbuf.size());
}



int daqb::perConnProcessRecvMesg(void* pconn, msgpack::object_handle& oh){ // IMPROVE IT AS A RING
  if(!m_isDataAccept){
    std::cout<< "msg is dropped"<<std::endl;
    return 0;
  }

  msgpack::object msg = oh.get();
  unique_zone& life = oh.zone();

  NetMsg netmsg;
  try{
    netmsg = std::move(msg.as<NetMsg>());
  }
  catch(...){
    std::cout<< "msg parsing error"<<std::endl;
    return 0;
  }

  // std::cout<< "bin "<<TcpConnection::binToHexString(netmsg.bin.data(),netmsg.bin.size())<<std::endl;
  if(netmsg.type!=NetMsg::Type::data){
    std::cout<< "unknown msg type"<<std::endl;
  }

  daqb_packSP df(new daqb_pack);

  df->headM = netmsg.headM;
  df->wireN = netmsg.wireN;
  df->hitN = netmsg.hitN;
  df->timeV = netmsg.timeV;

  m_st_n_ev_input_now ++;
  uint64_t next_p_ring_write = m_count_ring_write % m_size_ring;
  if(next_p_ring_write == m_hot_p_read){
    // buffer full, permanent data lose
    m_st_n_ev_overflow_now ++;
    return 0;
  }

  m_vec_ring_ev[next_p_ring_write] = df;
  m_count_ring_write ++;
  return 1;
}

std::string daqb::GetStatusString(){
  std::unique_lock<std::mutex> lk(m_mtx_st);
  return m_st_string;
}

daqb_packSP& daqb::Front(){
  if(m_count_ring_write > m_count_ring_read) {
    uint64_t next_p_ring_read = m_count_ring_read % m_size_ring;
    m_hot_p_read = next_p_ring_read;
    // keep hot read to prevent write-overlapping
    return m_vec_ring_ev[next_p_ring_read];
  }
  else{
    return m_ring_end;
  }
}

void daqb::PopFront(){
  if(m_count_ring_write > m_count_ring_read) {
    uint64_t next_p_ring_read = m_count_ring_read % m_size_ring;
    m_hot_p_read = next_p_ring_read;
    // keep hot read to prevent write-overlapping
    m_vec_ring_ev[next_p_ring_read].reset();
    m_count_ring_read ++;
  }
}

uint64_t daqb::Size(){
  return  m_count_ring_write - m_count_ring_read;
}

void daqb::ClearBuffer(){
  m_count_ring_write = m_count_ring_read;
  m_vec_ring_ev.clear();
}

uint64_t daqb::AsyncWatchDog(){
  std::chrono::system_clock::time_point m_tp_old;
  std::chrono::system_clock::time_point m_tp_run_begin;

  m_tp_run_begin = std::chrono::system_clock::now();
  m_tp_old = m_tp_run_begin;
  m_is_async_watching = true;

  m_st_n_tg_ev_old =0;
  m_st_n_ev_input_old = 0;
  m_st_n_ev_bad_old =0;
  m_st_n_ev_overflow_old = 0;

  while(m_is_async_watching){
    std::this_thread::sleep_for(std::chrono::seconds(1));
    uint64_t st_n_tg_ev_begin = m_st_n_tg_ev_begin;
    uint64_t st_n_tg_ev_now = m_st_n_tg_ev_now;
    uint64_t st_n_ev_input_now = m_st_n_ev_input_now;
    uint64_t st_n_ev_bad_now = m_st_n_ev_bad_now;
    uint64_t st_n_ev_overflow_now = m_st_n_ev_overflow_now;

    // time
    auto tp_now = std::chrono::system_clock::now();
    std::chrono::duration<double> dur_period_sec = tp_now - m_tp_old;
    std::chrono::duration<double> dur_accu_sec = tp_now - m_tp_run_begin;
    double sec_period = dur_period_sec.count();
    double sec_accu = dur_accu_sec.count();

    // period
    uint64_t st_n_tg_ev_period = st_n_tg_ev_now - m_st_n_tg_ev_old;
    uint64_t st_n_ev_input_period = st_n_ev_input_now - m_st_n_ev_input_old;
    uint64_t st_n_ev_bad_period = st_n_ev_bad_now - m_st_n_ev_bad_old;
    uint64_t st_n_ev_overflow_period = st_n_ev_overflow_now - m_st_n_ev_overflow_old;

    // ratio
    //double st_output_vs_input_accu = st_n_ev_input_now? st_ev_output_now / st_ev_input_now : 1;
    double st_bad_vs_input_accu = st_n_ev_input_now? 1.0 * st_n_ev_bad_now / st_n_ev_input_now : 0;
    double st_overflow_vs_input_accu = st_n_ev_input_now? 1.0 *  st_n_ev_overflow_now / st_n_ev_input_now : 0;
    double st_input_vs_trigger_accu = st_n_ev_input_now? 1.0 * st_n_ev_input_now / (st_n_tg_ev_now - st_n_tg_ev_begin + 1) : 1;
    //double st_output_vs_input_period = st_ev_input_period? st_ev_output_period / st_ev_input_period : 1;
    double st_bad_vs_input_period = st_n_ev_input_period? 1.0 * st_n_ev_bad_period / st_n_ev_input_period : 0;
    double st_overflow_vs_input_period = st_n_ev_input_period? 1.0 *  st_n_ev_overflow_period / st_n_ev_input_period : 0;
    double st_input_vs_trigger_period = st_n_tg_ev_period? 1.0 *  st_n_ev_input_period / st_n_tg_ev_period : 1;

    // hz
    double st_hz_tg_accu = (st_n_tg_ev_now - st_n_tg_ev_begin + 1) / sec_accu ;
    double st_hz_input_accu = st_n_ev_input_now / sec_accu ;

    double st_hz_tg_period = st_n_tg_ev_period / sec_period ;
    double st_hz_input_period = st_n_ev_input_period / sec_period ;

    std::string st_string_new =
      daqb::FormatString("L<%u> event(%d)/trigger(%d - %d)=Ev/Tr(%.4f) dEv/dTr(%.4f) tr_accu(%.2f hz) ev_accu(%.2f hz) tr_period(%.2f hz) ev_period(%.2f hz)",
                          m_extension, st_n_ev_input_now, st_n_tg_ev_now, st_n_tg_ev_begin, st_input_vs_trigger_accu, st_input_vs_trigger_period,
                          st_hz_tg_accu, st_hz_input_accu, st_hz_tg_period, st_hz_input_period
        );

    {
      std::unique_lock<std::mutex> lk(m_mtx_st);
      m_st_string = std::move(st_string_new);
    }

    //write to old
    m_st_n_tg_ev_old = st_n_tg_ev_now;
    m_st_n_ev_input_old = st_n_ev_input_now;
    m_st_n_ev_bad_old = st_n_ev_bad_now;
    m_st_n_ev_overflow_old = st_n_ev_overflow_now;
    m_tp_old = tp_now;
  }
  return 0;
}
