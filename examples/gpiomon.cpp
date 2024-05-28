/*
 * gpiomon.cpp
 *
 *  Created on: Oct 24, 2023
 *      Author: nbarros
 */


/*
* Placeholder PetaLinux user application.
*
* Replace this with your application code

* Copyright (C) 2013 - 2016  Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without restriction,
* including without limitation the rights to use, copy, modify, merge,
* publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in this
* Software without prior written authorization from Xilinx.
*
*/

extern "C"
{
  #include <unistd.h>
  #include <sys/mman.h>
  #include <fcntl.h>

};

#include <iostream>
#define __STDC_FORMAT_MACROS
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <thread>
#include <csignal>

#include "gpio_reg.h"
#include "globals.h"

// GPIO address
#define GPIO_DMA_BASE_ADDR 0xa0010000
#define GPIO_DMA_HIGH_ADDR 0xa001FFFF

// GPIO address for motor step counter
#define GPIO_MOTOR_BASE_ADDR 0xa0020000
#define GPIO_MOTOR_HIGH_ADDR 0xa002FFFF

// GPIO address for secondary aux monitor
#define GPIO_MON_BASE_ADDR 0xa0030000
#define GPIO_MON_HIGH_ADDR 0xa003FFFF


globals g_globals;
static volatile bool stop = false;


/*******************************************************************************************************************/
/* Handle a control C or kill, maybe the actual signal number coming in has to be more filtered?
 * The stop should cause a graceful shutdown of all the transfers so that the application can
 * be started again afterwards.
 */
void interrupt_handler(int a)
{
  printf("Received a signal [%d]\n",a);
  switch(a)
  {
  case SIGINT:
    printf("Signal %d is SIGINT\n",a);
    break;
  case SIGTERM:
    printf("Signal %d is SIGTERM\n",a);
    break;
  default:
    printf("Unknown signal %d\n",a);
  }
  printf("Stopping the run...\n");
  //stop_run((uint64_t)(gpio_1_mmaddr+0x0),&gpio_reg);

  stop = true;
}


/*******************************************************************************************************************/
/* Get the clock time in usecs to allow performance testing
 */
static uint64_t get_posix_clock_time_usec ()
{
  uint64_t val = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  return val;
}


int main(int argc, char *argv[])
{
  uint64_t start_time, end_time, time_diff;

  printf("Initiating GPIO probing...\n");


  signal(SIGINT, interrupt_handler);
  signal(SIGTERM,interrupt_handler);

  printf("Signals prepared\n");

  gpio_dev motor(GPIO_MOTOR_BASE_ADDR,GPIO_MOTOR_HIGH_ADDR,true);
  if (!motor.is_ready())
  {
    printf("--> Failed to map the device. Doing nothing\n");
    return 0;
  }

  gpio_dev monitor(GPIO_MON_BASE_ADDR,GPIO_MON_HIGH_ADDR,true);
  if (!monitor.is_ready())
  {
    printf("--> Failed to map the monitor device. Doing nothing\n");
    return 0;
  }

  // now enter a loop
  while (! stop)
  {

    //sleep(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //gpio_dev.read_register(0);
    printf("--> POS 0 : [%u] POS 1 : [%u]\t\t REG0 [%u] REG1 [%u]\n",
      motor.read_register(0),
      motor.read_register(1),
      monitor.read_register(0),
      monitor.read_register(1)
      );
  }

  printf("Terminating monitor...\n");
  // clean the registers
  motor.unmap_device();
  monitor.unmap_device();
  //g_globals.
  return 0;
}




