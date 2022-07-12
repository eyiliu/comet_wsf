#pragma once

#include "comet_wsf_system.hh"

namespace comet_wsf{
  class comet_regctrl{
  public:
    comet_regctrl();
    ~comet_regctrl();

    void daqb_reg_write(uint32_t o, uint32_t d);
    uint32_t daqb_reg_read(uint32_t o);
    uint32_t daqb_read_last_ack_cnt();
    void daqb_wait_for_ack_cnt(uint32_t last_cnt);

    void feb_cmd_write(uint16_t a, uint16_t d);
    double feb_read_new_temp();
    double feb_read_hv();
    void feb_set_hv_voltage(double v);
    void feb_set_dac_voltage(uint32_t ch, double v);
    void feb_set_asic_raw(uint32_t ch, uint16_t data);

    void daq_start_run();
    void daq_stop_run();
    void daq_reset();
    void daq_conf_default();

  private:
    uint32_t *pR_DAQB_VBASE32{nullptr};
    int fd{0};

  public:
    static void map_mem_block(int* p_fd, uint32_t paddr, uint32_t psize, uint32_t** pp_vbase32);
    static void ummap_mem_block(int* p_fd, uint32_t** pp_vbase32);
    static void daqb_reg_write(uint32_t o, uint32_t d, uint32_t* vbase);
    static uint32_t daqb_reg_read(uint32_t o, uint32_t* vbase);
  };
}
