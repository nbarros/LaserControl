/*
 * AD5339.cpp
 *
 *  Created on: Oct 20, 2023
 *      Author: nbarros
 */

#include <AD5339.h>

extern "C"
{
  #include <sys/ioctl.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <unistd.h>

  #include <linux/i2c-dev.h>
  #include <linux/i2c.h>
  //#include <i2c/smbus.h>

}
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <thread>
#include <chrono>

namespace cib
{

  AD5339::AD5339 (const uint16_t bus, const uint16_t address)
  : m_bus_addr(bus),
    m_dev_addr(address),
    m_dev(-1),
    m_is_ready(false),
    m_debug(true)
  {

    // open the service
    char fdev[32];
    sprintf(fdev,"/dev/i2c-%u",bus);
    m_dev = open(fdev,O_RDWR);
    if (m_dev < 0) {
      printf("AD5339::AD5339 : Failed to open the bus.\n");
      return;
    } else {
      if (m_debug) {
        printf("AD5339::AD5339 : Received fd %i\n",m_dev);
      }
      m_is_ready = true;
    }

    // -- check if one can query for functionality of the device
    // Extract the functions available
    uint64_t funcs = 0x0;
    int res = ioctl(m_dev, I2C_FUNCS, &funcs);
    if ( res < 0) {
      printf("AD5339::AD5339 : Failed to query device for functionality.\n");
      printf("AD5339::AD5339 : Msg : %s\n",std::strerror(errno));
    } else {
      printf("AD5339::AD5339 : Got func word for device : %" PRIu64 " (%" PRIx64 ")\n",funcs,funcs);
    }


    // read back the DAC current setting
    //printf("AD5339::AD5339 : Reading back inital value.\n");
    //m_dac.level.set_value(read_dac());

  }

  AD5339::~AD5339 ()
  {
    if (m_is_ready)
    {
      (void)close(static_cast<int>(m_dev));
    }
  }

  int AD5339::i2c_set_device(const uint8_t addr)
  {
    if (m_debug)
    {
      printf("AD5339::i2c_set_device : Setting device to %X\n",addr);
    }
    int ret = ioctl(m_dev,I2C_SLAVE,addr);
    int err = errno;
    if (ret < 0)
    {
      printf("AD5339::i2c_set_device : Failing addressing the appropriate slave register.\n");
      printf("message: %s\n",std::strerror(err));
      return 0;
    }
    return 1;
  }


  // the write operation should be reaaally low level
  // sufficiently so to be generic
  int AD5339::i2c_write(const int32_t fd, const uint8_t addr, uint8_t*buffer, const size_t nbytes, bool debug)
  {
    if (fd < 0 )
    {
      printf("i2c_write : Bus not ready\n");
      return 0;
    }
    //size_t bytes = sizeof(dac);
    if (debug)
    {
      printf("==================================================================\n");
      printf("i2c_write : Writing contents to I2C DAC \n");
      printf(" * device : [fd %i] [DAC 0x%X]\n",fd,addr);
      printf(" * Full buffer :\n");
      for (size_t idx = 0; idx < nbytes; idx++)
      {
        printf(" Byte %lu : 0x%X\n",idx,buffer[idx]);
      }
      printf("==================================================================\n");
    }

    // set the device
    if (!i2c_set_device(addr))
    {
      printf("i2c_write : Failed to set device\n");
      return 0;
    }
    // now we can write the stuff
    ssize_t wr_bytes = write(fd,buffer,nbytes);
    if (wr_bytes != nbytes)
    {
      printf("i2c_write : Failed to write the message to DAC 0x%X .\n",
             m_dev_addr);
      printf("i2c_write : Received %li (expected %lu)\n",
             wr_bytes,
             nbytes);
      // FIXME: Should we clear the DAC?
      return 0;
    }
    else
    {
      if(debug)
      {
        printf("i2c_write : Message written\n");
      }
    }
    return 1;
  }


  int AD5339::i2c_read(dac_t &dac)
  {
    if (!m_is_ready)
    {
      printf("AD5339::i2c_read : Bus not ready\n");
      return 0;
    }

    if (m_debug) {
      printf("==================================================================\n");
      printf("i2c_read : Reading contents from I2C DAC 0x%X:%u:\n",m_dev_addr,dac.get_channel());
      printf(" * device : [fd %i] [DAC 0x%X:%u ]\n",m_dev, m_dev_addr,
             dac.get_channel());
      printf("==================================================================\n");
    }


    // the read operation cannot be performed on both DAC channels
    // therefore check that the channel has a valid value
    if (dac.get_channel() > 2)
    {
      printf("i2c_read : Invalid channe. Got %u (expected value in [1,2])\n",dac.get_channel());
      return 0;
    }

    //
    // -- setting the pointer already includes setting the address
    //
    //
//    // -- Set the address first
//    if (!i2c_set_device())
//    {
//      printf("i2c_read : Failed to set device address.\n");
//      return 0;
//    }

    // -- unlike the write operation, this is a two step process
    // step 1:  write the pointer byte
    if (!i2c_write(m_dev,m_dev_addr,&dac.ptr,sizeof(dac.ptr),m_debug))
    {
      printf("i2c_read : Failed to update device pointer.\n");
      return 0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));


    // now make the real read request
    size_t nbytes = sizeof(dac.level);
    uint8_t *buffer = new uint8_t[nbytes];
    ssize_t rd_bytes = read(m_dev,buffer,nbytes);
    if (rd_bytes != sizeof(dac.level))
    {
      printf("i2c_read : Failed to read the register from DAC 0x%X:%u. Received %li (expected %lu).\n",
             m_dev_addr,dac.get_channel(),rd_bytes,nbytes);
      return 0;
    }

    if (m_debug)
    {
      printf("i2c_read : Contents from register 0x%X:%u\n",m_dev_addr,dac.get_channel());
      for (size_t idx = 0; idx < nbytes; idx++) {
        printf("[0x%X] ",buffer[idx]);
      }
      printf("\n");
    }

    // copy the contents to the dac structure
    dac.level.msb = buffer[0];
    dac.level.lsb = buffer[1];
    delete [] buffer;

    return 1;
  }

  int AD5339::write_dac(const uint16_t val, const uint16_t ch)
   {
     if (!m_is_ready)
     {
       printf("AD5339::write_dac : Bus is not ready");
       return 0;
     }

     dac_t dac;
     dac.level.set_value(val);
     dac.set_channel(ch);

     if (m_debug)
     {
       printf("==================================================================\n");
       printf("AD5339::write_dac : Writing contents to I2C DAC 0x%X:%u:\n",m_dev_addr,ch);
       printf(" * device : [fd %i] [DAC 0x%X %u ]\n",m_dev,
              m_dev_addr,
              dac.get_channel());

       printf(" * Values : [lvl value 0x%X (%u)] [msb 0x%X (%u)] [lsb 0x%X (%u)]\n",
              dac.level.get_value(),
              dac.level.get_value(),
              dac.level.msb,
              dac.level.msb,
              dac.level.lsb,
              dac.level.lsb);
       printf("==================================================================\n");

     }


     if (!i2c_write(m_dev,m_dev_addr,reinterpret_cast<uint8_t*>(&dac),sizeof(dac),m_debug))
     {
       printf("AD5339::write_dac : Failed to write to register\n");
       return 0;
     }

     return 1;
   }

  int AD5339::read_dac(const uint16_t ch)
  {

    if (!m_is_ready)
    {
      printf("AD5339::read_dac : Bus is not ready\n");
      return -1;
    }

    // check that channel is either 1 or 2
    if (ch > 2)
    {
      printf("AD5339::read_dac : Invalid channel. Valid values are [1,2].\n");
      return -1;
    }

    // set the pointer byte
    dac_t dac;
    dac.set_channel(ch);

    // read the thingy
    if (!i2c_read(dac))
    {
      printf("AD5339::read_dac : Failed to read the DAC.\n");
      return -1;
    }

    return dac.level.get_value();
  }

} /* namespace cib */
