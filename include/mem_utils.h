/*
 * mem_utils.h
 *
 *  Created on: Mar 15, 2024
 *      Author: Nuno Barros
 */

#ifndef DEVICE_INCLUDE_MEM_UTILS_H_
#define DEVICE_INCLUDE_MEM_UTILS_H_
#include <cstdint>
#include <cstdio>

using std::size_t;
namespace cib
{
  namespace util
  {
    uintptr_t map_phys_mem(uintptr_t base_addr,uintptr_t high_addr);
    int unmap_mem(void* virt_addr, size_t size);
    uint32_t reg_read(uintptr_t addr);
    void reg_write(uintptr_t addr, uint32_t value);
    void* cast_to_void(uintptr_t val);
//    uint32_t cast_to_uint(void* val);
    uintptr_t cast_to_uintptr(void* val);

  }
};

#endif /* DEVICE_INCLUDE_MEM_UTILS_H_ */
