/*
 * AD5339.h
 *
 *  Created on: Oct 20, 2023
 *      Author: nbarros
 */

#ifndef INCLUDE_AD5339_H_
#define INCLUDE_AD5339_H_

#include <cinttypes>
#include <cstring>

#define DACNCLRBIT 0x5
#define DACNLOADBIT 0x4
#define DACNCLR (1UL << DACNCLRBIT)
#define DACNLOAD (1UL << DACNLOADBIT)

namespace cib
{

  typedef struct level_t
  {
    uint8_t msb;
    uint8_t lsb;

    void set_clear(bool clear)
    {
      msb = (msb & ~((uint8_t)DACNCLR)) | ((uint8_t)!clear << DACNCLRBIT);
    }

    void set_load(bool load)
    {
      msb = (msb & ~((uint8_t)DACNLOAD)) | ((uint8_t)!load << DACNLOADBIT);
    }

    void set_value(const uint16_t val, bool clear = false, bool load = true)
    {
      msb = ((val >> 8) & 0xF);
      set_clear(clear);
      lsb = (val & 0xFF);
    }

    const uint16_t get_value()
    {
      uint16_t val = msb & 0xF;
      val = (val << 8) | lsb;
      return val;
    }

  } level_t;

  typedef struct dac_t
  {
    uint8_t ptr;
    level_t level;

    void set_channel(const uint16_t ch)
    {
      ptr = static_cast<uint8_t>(ch & 0xF);
    }

    const uint16_t get_channel()
    {
      return static_cast<uint16_t>(ptr);
    }
  } dac_t;
  /*
   *
   */
  class AD5339 final
  {
  public:

    enum channel_t {kC1=1, kC2=2, kC12 = 3};

    AD5339 (const uint16_t bus = 8, const uint16_t address = 0xD);
    virtual ~AD5339 ();
    AD5339 (const AD5339 &other) = delete;
    AD5339 (AD5339 &&other) = delete;
    AD5339& operator= (const AD5339 &other) = delete;
    AD5339& operator= (AD5339 &&other) = delete;

    int read_dac(const enum channel_t ch = kC1) {return read_dac(static_cast<uint16_t>(ch));}
    int read_dac(const uint16_t ch);
    int write_dac(const uint16_t val, const enum channel_t ch = kC1) {return write_dac(val,static_cast<uint16_t>(ch));};
    int write_dac(const uint16_t val, const uint16_t ch);

    int clear_dac(const uint16_t ch);
    //{m_dac.level.set_clear(true); return i2c_write(m_dac);}

    bool is_ready() const {return m_is_ready;}

    void set_debug(bool debug=true) {m_debug = debug;}
    bool get_debug() const {return m_debug;}
  private:
    //const char    *m_bus_addr_str = ;

    int i2c_set_device(const uint8_t addr);
    int i2c_set_device() {return i2c_set_device(m_dev_addr);};
    int i2c_write(const int32_t fd, const uint8_t addr, uint8_t*buffer, const size_t nbytes, bool debug = false);

    int i2c_read(dac_t &dac);


    uint16_t  m_bus_addr;
    uint8_t   m_dev_addr;
    int32_t   m_dev;
    bool      m_is_ready;
    bool      m_debug;
    //dac_t     m_dac;
  };

} /* namespace cib */

#endif /* INCLUDE_AD5339_H_ */
