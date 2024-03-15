/*
 * mem_utils.cpp
 *
 *  Created on: Mar 15, 2024
 *      Author: Nuno Barros
 */

#include <mem_utils.h>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <sys/mman.h>

extern "C"
{
#include <fcntl.h>
#include <inttypes.h>
};
namespace cib
{
  namespace util
  {

    volatile int g_mmap_fd;


    uintptr_t map_phys_mem(uintptr_t base_addr,uintptr_t high_addr)
    {
      void * mapped_addr = nullptr;
      off_t dev_base = base_addr;
      if (g_mmap_fd == 0)
      {
        g_mmap_fd = open("/dev/mem",O_RDWR | O_SYNC);
        if (g_mmap_fd == -1)
        {
          printf("Failed to map register [0x%" PRIx64 " 0x%" PRIx64 "].",base_addr,high_addr);
          g_mmap_fd = 0;
          return NULL;
        }
      }
      // Map into user space the area of memory containing the device
      mapped_addr = mmap(0, (high_addr-base_addr), PROT_READ | PROT_WRITE, MAP_SHARED, g_mmap_fd, dev_base & ~(high_addr-base_addr-1));
      if ( (intptr_t)mapped_addr == -1) {
        printf("Failed to map register [0x%" PRIx64 " 0x%" PRIx64 "] into virtual address.",base_addr,high_addr);
        mapped_addr = NULL;
      }
      return reinterpret_cast<uintptr_t>(mapped_addr);
    }

    int unmap_mem(void* virt_addr, size_t size)
    {
      if (virt_addr == nullptr)
      {
        return 0;
      }
      return munmap(virt_addr,size);
    }

    uint32_t reg_read(uintptr_t addr)
    {
      return *(volatile uintptr_t*) addr;
      //*static_cast<volatile uint32_t*>(cast_to_void())
    }

    void reg_write(uintptr_t addr, uint32_t value)
    {
      *(volatile uint32_t*) addr = value;
    }

    // take an address and cast it to void*
    // the interesting part is that we need to make sure that
    void* cast_to_void(uintptr_t val)
    {
      return reinterpret_cast<void *>(val);
    }

//    uint32_t cast_to_uint(void* val)
//    {
//      return reinterpret_cast<uint32_t>(val);
//    }

    uintptr_t cast_to_uintptr(void* val)
    {
      return reinterpret_cast<uintptr_t>(val);
    }

    void reg_write_mask(uintptr_t addr, uint32_t value, uint32_t mask)
    {
      uint32_t cache = reg_read(addr);
      reg_write(addr,((cache & ~mask) | (value & mask)));
    }

  };
};



