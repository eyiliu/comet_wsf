#pragma once

#include <mutex>
#include <future>
#include <memory>

#include <cstdio>

// #include "TelEvent.hpp"
#include "TcpConnection.hh"


namespace comet_wsf{

  struct daqb_pack{
    uint16_t headM;
    uint16_t wireN;
    uint16_t hitN;
    uint64_t timeV;
  };


  using daqb_packSP = std::shared_ptr<daqb_pack>;

  class daqb{
  public:
    std::future<uint64_t> m_fut_async_watch;
    std::vector<daqb_packSP> m_vec_ring_ev;
    daqb_packSP m_ring_end;

    uint64_t m_size_ring{200000};
    std::atomic<uint64_t> m_count_ring_write;
    std::atomic<uint64_t> m_hot_p_read;
    uint64_t m_count_ring_read;
    bool m_is_async_watching{false};

    uint64_t m_extension{0};

    //status variable:
    std::atomic<uint64_t> m_st_n_tg_ev_now{0};
    std::atomic<uint64_t> m_st_n_ev_input_now{0};
    std::atomic<uint64_t> m_st_n_ev_output_now{0};
    std::atomic<uint64_t> m_st_n_ev_bad_now{0};
    std::atomic<uint64_t> m_st_n_ev_overflow_now{0};
    std::atomic<uint64_t> m_st_n_tg_ev_begin{0};

    uint64_t m_st_n_tg_ev_old{0};
    uint64_t m_st_n_ev_input_old{0};
    uint64_t m_st_n_ev_bad_old{0};
    uint64_t m_st_n_ev_overflow_old{0};

    std::string m_st_string;
    std::mutex m_mtx_st;

    uint32_t m_tg_expected{0};
    uint32_t m_flag_wait_first_event{true};

    bool m_isDataAccept{false};
    std::unique_ptr<TcpConnection> m_conn;
public:
    daqb(const std::string& name, const std::string& host, short int port)
      :m_name(name), m_host(host), m_port(port){
    }


    ~daqb();

    void daq_start_run();
    void daq_stop_run();
    void daq_reset();
    void daq_conf_default();

    int perConnProcessRecvMesg(void* pconn, msgpack::object_handle& oh);

    daqb_packSP& Front();
    void PopFront();
    uint64_t Size();
    void ClearBuffer();

    std::string GetStatusString();
    uint64_t AsyncWatchDog();


    std::string m_name;
    std::string m_host;
    short int m_port;
    std::vector<std::pair<uint16_t, uint16_t>> m_hots;


    template<typename ... Args>
    static std::string FormatString( const std::string& format, Args ... args ){
      std::size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
      std::unique_ptr<char[]> buf( new char[ size ] );
      std::snprintf( buf.get(), size, format.c_str(), args ... );
      return std::string( buf.get(), buf.get() + size - 1 );
    }

  };
}
