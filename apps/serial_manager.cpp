/*
 * serial_manager.cpp
 *
 *  Created on: May 27, 2024
 *      Author: Nuno Barros
 */



#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <spdlog/spdlog.h>
#include <cerrno>
#include <string>
extern "C"
{
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
};

// control library utilities
#include <utilities.hh>
#include <spdlog/spdlog.h>

#include <Laser.hh>
#include <Attenuator.hh>
#include <PowerMeter.hh>

typedef struct iols_dev_t
{
  std::string serial_nr;
  std::string port;
  uint32_t baud_rate;
  bool port_valid() {return (port.size() != 0);}
} iols_dev_t;

typedef struct iols_config_t
{
  iols_dev_t laser;
  iols_dev_t attenuator;
  iols_dev_t power_meter;
}iols_config_t;

typedef struct iolaser_t
{
  iols_config_t config;
  device::Attenuator *attenuator;
  device::Laser *laser;
  device::PowerMeter *power_meter;

} iolaser_t;


iolaser_t iols;

#define LASER_SN  "A9J0G3SV"
#define PM_SN     "A9CQTZ05"
#define ATT_SN     "6ATT1788D"

bool g_ignore_laser;
bool g_ignore_pm;
bool g_ignore_attenuator;
//
// Prototypes
//
int run_command(int argc, char** argv);


int map_laser()
{
  int ret = 0;
  spdlog::info("Initializing the laser");
  iols.config.laser.port = util::find_port(iols.config.laser.serial_nr);
  if (!iols.config.laser.port_valid())
  {
    spdlog::error("Failed to find Laser. Expected to see a device with serial number {0}",iols.config.laser.serial_nr);
    util::enumerate_ports();
    return 1;
  }
  spdlog::trace("Found port {0}",iols.config.laser.port);
  try
  {
    spdlog::trace("Creating device instance");
    iols.laser = new device::Laser(iols.config.laser.port.c_str(),iols.config.laser.baud_rate);
    spdlog::trace("Instance created");

    // -- this sets the cache variables so that we can get a whole ton of stuff out of it
    spdlog::debug("Setting response timeout to 100 ms. laser is slow!!!");
    iols.laser->set_timeout_ms(100);

    spdlog::info("Checking interlocks...");
    std::string code;
    std::string desc;
    iols.laser->security(code, desc);
    spdlog::info("Laser responded [{0} : {1}]",code,desc);

    spdlog::trace("Getting current shot count...");
    uint32_t count;
    iols.laser->get_shot_count(count);
    spdlog::info("Laser shot count [{0}]",count);

    spdlog::info("Laser initialized. Ready for operation");
  }
  catch(serial::PortNotOpenedException &e)
  {
    spdlog::critical("Port not open exception : {0}",e.what());
    ret = 1;
  }
  catch(serial::SerialException &e)
  {
    spdlog::critical("Serial exception : {0}",e.what());
    ret = 1;
  }
  catch(std::exception &e)
  {
    spdlog::critical("STL exception : {0}",e.what());
    ret = 1;
  }
  catch(...)
  {
    spdlog::critical("Caught an unexpected exception");
    ret = 1;
  }

  return ret;
}


int query_attenuator_settings()
{
  if (g_ignore_attenuator)
  {
    spdlog::error("Attenuator is being ignored.No operations possible");
    return 1;
  }
  int ret = 0;

  if (iols.attenuator == nullptr)
  {
    spdlog::error("There is no open instance of the attenuator");
    return 1;
  }
  try
  {
    // -- this sets the cache variables so that we can get a whole ton of stuff out of it
    spdlog::debug("Refreshing status");
    iols.attenuator->refresh_status();

    // the attenuator is actually pretty cool in terms of interface
    spdlog::debug("Querying serial number...");
    std::string sn;
    iols.attenuator->get_serial_number(sn);
    spdlog::debug("Response [{0}]",sn);
    // -- if this somehow fails,it may mean that the string configuration is wrong

    spdlog::debug("Querying status in human form...");
    std::string st = iols.attenuator->get_status_raw();
    spdlog::debug("Response [{0}]",st);

    // grab all the settings separately:
    spdlog::debug("Querying individual settings (to see they were properly parsed):");
    int32_t pos;
    uint16_t status;
    iols.attenuator->get_position(pos, status, false);
    spdlog::debug("Position     : {0}",pos);
    spdlog::debug("Status       : {0}",status);
    int32_t offset;
    iols.attenuator->get_offset(offset);
    spdlog::debug("Offset       : {0}",offset);
    uint32_t speed;
    iols.attenuator->get_max_speed(speed);
    spdlog::debug("Max speed    : {0}",speed);
    device::Attenuator::OpMode mode;
    iols.attenuator->get_op_mode(mode);
    spdlog::debug("Op mode      : {0}",static_cast<int>(mode));
    uint16_t acc, dec, res, cur_idle, cur_mov;
    iols.attenuator->get_acceleration(acc);
    spdlog::debug("Acceleration : {0}",acc);
    iols.attenuator->get_deceleration(dec);
    spdlog::debug("Deceleration : {0}",dec);
    iols.attenuator->get_resolution(res);
    spdlog::debug("Resolution   : {0}",res);
    iols.attenuator->get_current_idle(cur_idle);
    spdlog::debug("Idle current : {0}",cur_idle);
    iols.attenuator->get_current_move(cur_mov);
    spdlog::debug("Move current : {0}",cur_mov);
  }
  catch(serial::PortNotOpenedException &e)
  {
    spdlog::critical("Port not open exception : {0}",e.what());
    ret = 1;
  }
  catch(serial::SerialException &e)
  {
    spdlog::critical("Serial exception : {0}",e.what());
    ret = 1;
  }
  catch(std::exception &e)
  {
    spdlog::critical("STL exception : {0}",e.what());
    ret = 1;
  }
  catch(...)
  {
    spdlog::critical("Caught an unexpected exception");
    ret = 1;
  }

  return ret;
}
int map_attenuator()
{
  int ret = 0;
  if (g_ignore_attenuator)
  {
    spdlog::error("Attenuator is being ignored.No operations possible");
    return 1;
  }

  if (iols.attenuator == nullptr)
  {
    spdlog::error("There is no open instance of the attenuator");
    return 1;
  }

  spdlog::info("Initializing the attenuator");
  iols.config.attenuator.port = util::find_port(iols.config.attenuator.serial_nr);
  if (!iols.config.attenuator.port_valid())
  {
    spdlog::error("Failed to find Attenuator. Expected to see a device with serial number {0}",iols.config.attenuator.serial_nr);
    util::enumerate_ports();
    return 1;
  }
  spdlog::trace("Found port {0}",iols.config.attenuator.port);
  try
  {
    spdlog::trace("Creating device instance");
    iols.attenuator = new device::Attenuator(iols.config.attenuator.port.c_str(),iols.config.attenuator.baud_rate);
    spdlog::trace("Instance created");

    ret = query_attenuator_settings();
    if (ret != 0)
    {
      spdlog::error("Failed to query attenuator");
      ret = 1;
    }
    else
    {
      spdlog::info("Attenuator initialization done. Can't test anything else without actually changing settings.\n\n\n\n");
    }
  }
  catch(serial::PortNotOpenedException &e)
  {
    spdlog::critical("Port not open exception : {0}",e.what());
    ret = 1;
  }
  catch(serial::SerialException &e)
  {
    spdlog::critical("Serial exception : {0}",e.what());
    ret = 1;
  }
  catch(std::exception &e)
  {
    spdlog::critical("STL exception : {0}",e.what());
    ret = 1;
  }
  catch(...)
  {
    spdlog::critical("Caught an unexpected exception");
    ret = 1;
  }

  return ret;
}
int map_power_meter()
{
  int ret = 0;
  if (g_ignore_pm)
  {
    spdlog::error("Power Meter is being ignored.No operations possible");
    return 1;
  }

  if (iols.power_meter == nullptr)
  {
    spdlog::error("There is no open instance of the power meter");
    return 1;
  }

  iols.config.power_meter.port = util::find_port(iols.config.power_meter.serial_nr);
  if (!iols.config.power_meter.port_valid())
  {
    spdlog::error("Failed to find PowerMeter. Expected to see a device with serial number {0}",iols.config.power_meter.serial_nr);
    util::enumerate_ports();
    return 1;
  }
  spdlog::trace("Found port {0}",iols.config.power_meter.port);

  try
  {
    spdlog::trace("Creating device instance");
    iols.power_meter = new device::PowerMeter(iols.config.power_meter.port.c_str(),iols.config.power_meter.baud_rate);
    spdlog::trace("Instance created");

    // do some query on functionality
    spdlog::debug("Querying head information");
    std::string tp, sn,name;
    bool pw,en,fq;
    iols.power_meter->head_info(tp,sn,name,pw,en,fq);
    spdlog::debug("HEAD INFO :\n"
        "Type           : {0}\n"
        "Serial number  : {1}\n"
        "Name           : {2}\n"
        "Power capable  : {3}\n"
        "Energy capable  : {4}\n"
        "Frequency capable  : {5}\n"
        , tp,sn,pw,en,fq);
    spdlog::debug("Querying the instrument:");
    iols.power_meter->inst_info(tp, sn, name);
    spdlog::debug("INSTRUMENT INFO :\n"
        "ID             : {0}\n"
        "Serial number  : {1}\n"
        "Name           : {2}\n"
        ,tp,sn,name);
    spdlog::debug("Querying range:");
    int16_t cur;
    std::map<int16_t,std::string> rmap;
    iols.power_meter->get_all_ranges(cur);
    iols.power_meter->get_range_map(rmap);
    spdlog::debug("Current range [{0}]",cur);
    spdlog::debug("RANGES :");
    for (auto item : rmap)
    {
      spdlog::debug("{0} : {1}",item.first,item.second );
    }

    // now query the user threshold
    uint16_t current,min,max;
    spdlog::debug("Checking user threshold");
    iols.power_meter->query_user_threshold(current, min, max);
    spdlog::debug("Response : {0} : [{1} , {2}]",current,min,max);

    // get pulse widths
    std::map<uint16_t,std::string> umap;
    spdlog::debug("Checking pulse options");
    iols.power_meter->pulse_length(0, current);
    iols.power_meter->get_pulse_map(umap);
    spdlog::debug("Current pulse width [{0}]",current);
    spdlog::debug("OPTIONS :");
    for (auto item : umap)
    {
      spdlog::debug("{0} : [{1}]",item.first,item.second);
    }

    // get wavelengths
    spdlog::debug("Querying wavelength capabilities (method 1)");
    iols.power_meter->get_all_wavelengths(name);
    spdlog::debug("Raw answer [{0}]",name);

    // Not implemented
    //     cout<< log(label) << "Querying wavelength capabilities (method 2)" << endl;
    //     std::string raw_answer;
    //     m_pm->get_all_wavelengths(raw_answer);
    //     cout << log(label) << "Current selection : " << cur << endl;


    // averaging settings
    spdlog::debug("Querying averaging settings");
    iols.power_meter->average_query(0, current);
    // -- this actually also fills the map on the first execution
    iols.power_meter->get_averages_map(umap);
    spdlog::debug("Current average setting [{0}]",current);
    spdlog::debug("OPTIONS :");
    for (auto item : umap)
    {
      spdlog::debug("{0} : [{1}]",item.first,item.second);
    }

    // max frequency
    spdlog::debug("Querying for max frequency");
    uint32_t u32;
    iols.power_meter->max_freq(u32);
    spdlog::debug("Answer : {0}",u32);
    spdlog::debug("Base assessments done. Can't test anything else without actually changing settings.\n\n\n\n");

  }
  catch(serial::PortNotOpenedException &e)
  {
    spdlog::critical("Port not open exception : {0}",e.what());
    ret = 1;
  }
  catch(serial::SerialException &e)
  {
    spdlog::critical("Serial exception : {0}",e.what());
    ret = 1;
  }
  catch(std::exception &e)
  {
    spdlog::critical("STL exception : {0}",e.what());
    ret = 1;
  }
  catch(...)
  {
    spdlog::critical("Caught an unexpected exception");
    ret = 1;
  }
  return ret;
}
int map_devices()
{
  int ret = 0;
  if (g_ignore_attenuator)
  {
    spdlog::warn("Attenuator is being ignored. Skipping mapping");
  }
  else
  {
    if (iols.attenuator == nullptr)
    {
      spdlog::error("There is no open instance of the attenuator");
      return 1;
    }

    spdlog::debug("Mapping the attenuator");
    ret = map_attenuator();
    if (ret != 0)
    {
      spdlog::critical("Failed to initialize the attenuator");
    }
  }

  if (g_ignore_laser)
  {
    spdlog::warn("Laser is being ignored. Skipping mapping");
  }
  else
  {
    if (iols.laser == nullptr)
    {
      spdlog::error("There is no open instance of the laser");
      return 1;
    }

    spdlog::debug("Mapping the laser");
    ret = map_laser();
    if (ret != 0)
    {
      spdlog::critical("Failed to initalize the laser");
    }
  }
  if (g_ignore_pm)
  {
    spdlog::warn("Attenuator is being ignored.No operations possible");
  }
  else
  {
    if (iols.power_meter == nullptr)
    {
      spdlog::error("There is no open instance of the power meter");
      return 1;
    }

    spdlog::debug("Mapping the power meter");
    ret = map_power_meter();
    if (ret != 0)
    {
      spdlog::critical("Failed a query to the power meter");
    }
  }
  return ret;
}

int unmap_devices()
{
  spdlog::info("Destroying the device instances");
  spdlog::trace("Destroying the power meter");
  if (iols.power_meter)
  {
    delete iols.power_meter;
    spdlog::trace("power meter instance destroyed");
  }
  spdlog::trace("Destroying the attenuator");
  if (iols.attenuator)
  {
    delete iols.attenuator;
    spdlog::trace("attenuator instance destroyed");
  }
  spdlog::trace("Destroying the laser");
  if (iols.laser)
  {
    // actually, make sure that the shutter is closed and firing is happening
    iols.laser->fire_stop();
    iols.laser->shutter_close();
    delete iols.laser;
    spdlog::trace("laser instance destroyed");
  }
  return 0;

}


void print_help()
{
  spdlog::info("Available commands (note, commands without arguments print current settings):");
  if (!g_ignore_laser)
  {
    spdlog::info("  laser subcmd [args]");
    spdlog::info("    Available subcomands:");
    spdlog::info("      single_shot");
    spdlog::info("        Fires a single shot");
    spdlog::info("      shot_count");
    spdlog::info("        Gets the shot count");
    spdlog::info("      security");
    spdlog::info("        Gets the security code and description");
    spdlog::info("      fire_start");
    spdlog::info("        Starts firing using the internal clock");
    spdlog::info("      fire_stop");
    spdlog::info("        Stops firing with the internal clock");
    spdlog::info("      prescale <value>");
    spdlog::info("        Sets the prescale");
    spdlog::info("      division <value>");
    spdlog::info("        Sets the pulse division");
    spdlog::info("      hv <value>");
    spdlog::info("        Sets the HV value (in kV as a float)");
    spdlog::info("      qswitch <value>");
    spdlog::info("        Sets qswitch value (in us)");
  }
  if (!g_ignore_attenuator)
  {
    spdlog::info("  attenuator [subcmd [args]]");
    spdlog::info("    Without arguments queries config");
    spdlog::info("    Available subcomands:");
    spdlog::info("      set_zero");
    spdlog::info("        Set current position to zero");
    spdlog::info("      stop");
    spdlog::info("        Stops motor movement");
    spdlog::info("      go_home");
    spdlog::info("        Go to zero position");
    spdlog::info("      get_position");
    spdlog::info("        Gets current position");
    spdlog::info("      reset");
    spdlog::info("        Reset the controller");
    spdlog::info("      get_settings");
    spdlog::info("        Gets (and prints) the current settings");
    spdlog::info("      move <value>");
    spdlog::info("        Relative move by number of steps (positive or negative)");
    spdlog::info("      move_to <value>");
    spdlog::info("        Move to position");
    spdlog::info("      set_resolution <value>");
    spdlog::info("        Set resolution");
    spdlog::info("      set_idle_current <value>");
    spdlog::info("        Set idle current");
    spdlog::info("      set_move_current <value>");
    spdlog::info("        Set moving current");
    spdlog::info("      set_acceleration <value>");
    spdlog::info("        Set acceleration");
    spdlog::info("      set_deceleration <value>");
    spdlog::info("        Set deceleration");
    spdlog::info("      set_max_speed <value>");
    spdlog::info("        Set max speed");
  }
  if (!g_ignore_pm)
  {
    spdlog::info("  power_meter");
    spdlog::info("    ** This is not implemented yet **");
  }
  spdlog::info("  help");
  spdlog::info("    Print this help");
  spdlog::info("  exit");
  spdlog::info("    Closes the command interface");
  spdlog::info("");

}


int run_command(int argc, char** argv)
{
  if (argc< 1)
  {
    return 1;
  }
  try
  {
    std::string cmd(argv[0]);
    // check command request
    if (cmd == "exit")
    {
      return 255;
    }
    else if (cmd == "help")
    {
      print_help();
      return 0;
    }
    else if (cmd == "laser")
    {
      if (g_ignore_laser)
      {
        spdlog::error("Laser is being ignored.No operations possible");
        return 0;
      }
      if (iols.laser == nullptr)
      {
        spdlog::error("There is no open instance of the laser");
        return 0;
      }
      int res = 0;
      if (argc == 2)
      {
        if (std::string(argv[1]) == "single_shot")
        {
          spdlog::warn("Firing a single shot");
          iols.laser->single_shot();
        }
        else if (std::string(argv[1]) == "shot_count")
        {
          uint32_t count;
          iols.laser->get_shot_count(count);
          spdlog::info("COUNT : {0}",count);
        }
        else if (std::string(argv[1]) == "security")
        {
          std::string code, msg;
          iols.laser->security(code,msg);
          spdlog::info("SECURITY : {0} :{1}",code,msg);
        }
        else if (std::string(argv[1]) == "fire_start")
        {
          iols.laser->fire_start();
          spdlog::warn("Laser started firing");
        }
        else if (std::string(argv[1]) == "fire_stop")
        {
          iols.laser->fire_stop();
          spdlog::warn("Laser stopped firing");
        }
        else if (std::string(argv[1]) == "shutter_open")
        {
          iols.laser->shutter_open();
          spdlog::warn("Shutter is open");
        }
        else if (std::string(argv[1]) == "shutter_close")
        {
          iols.laser->shutter_close();
          spdlog::warn("Shutter is closed");
        }
        else
        {
          spdlog::error("Unknown command {0}",argv[1]);
          print_help();
        }
      }
      else if (argc == 3)
      {
        if (std::string(argv[1]) == "prescale")
        {
          uint32_t ps = std::strtol(argv[2],NULL,0);
          spdlog::debug("Setting prescale to {0}",ps);
          iols.laser->set_prescale(ps);
        }
        else if (std::string(argv[1]) == "division")
        {
          uint32_t div = std::strtol(argv[2],NULL,0);
          spdlog::debug("Setting pulse division to {0}",div);
          iols.laser->set_pulse_division(div);
        }
        else if (std::string(argv[1]) == "hv")
        {
          float hv = std::stof(argv[2]);
          spdlog::debug("Setting pump HV to {0}",hv);
          iols.laser->set_pump_voltage(hv);
        }
        //      else if (std::string(argv[1]) == "set_rate")
        //      {
        //        float rate = std::stof(argv[2]);
        //        spdlog::warn("Setting repetition rate to {0}",rate);
        //        iols.laser->set_repetition_rate(rate);
        //      }
        else if (std::string(argv[1]) == "qswitch")
        {
          uint32_t qs = std::strtol(argv[2],NULL,0);
          spdlog::warn("Setting qswitch to {0} us",qs);
          iols.laser->set_qswitch(qs);
        }
        else
        {
          spdlog::error("Unknown command {0}",argv[1]);
          print_help();
        }
      }
      else
      {
        spdlog::error("Unknown laser command");
        print_help();
      }
      return 0;
    }
    else if (cmd == "attenuator")
    {
      if (g_ignore_attenuator)
      {
        spdlog::error("Attenuator is being ignored.No operations possible");
        return 0;
      }
      if (iols.attenuator == nullptr)
      {
        spdlog::error("There is no open instance of the attenuator");
        return 0;
      }

      int res = 0;
      if (argc == 1)
      {
        res = query_attenuator_settings();
        if (res != 0)
        {
          spdlog::error("Failed to query settings");
          return 1;
        }
      }
      if (argc == 2)
      {
        if (std::string(argv[1]) == "set_zero")
        {
          spdlog::info("Setting zero to current position");
          iols.attenuator->set_zero();
        }
        else if (std::string(argv[1]) == "stop")
        {
          // this is a soft stop
          spdlog::warn("stopping movement");
          iols.attenuator->stop(false);
        }
        else if (std::string(argv[1]) == "go_home")
        {
          spdlog::info("Going home");
          iols.attenuator->go_home();
        }
        else if (std::string(argv[1]) == "get_position")
        {
          spdlog::info("Getting position");
          int32_t position;
          uint16_t status;
          iols.attenuator->get_position(position,status);
          spdlog::info("POS : {0} (status :{1}",position,status);
        }
        else if (std::string(argv[1]) == "reset")
        {
          spdlog::info("Resetting controller");
          iols.attenuator->reset_controller();
        }
        else if (std::string(argv[1]) == "get_settings")
        {
          spdlog::info("Querying settings");
          std::string sn;
          int32_t offset;
          uint32_t speed, max_speed;
          uint16_t acceleration, deceleration, resolution, current_idle, current_move;
          device::Attenuator::OpMode mode;
          device::Attenuator::MotorState state;
          iols.attenuator->refresh_status();
          iols.attenuator->get_serial_number(sn);
          iols.attenuator->get_offset(offset);
          iols.attenuator->get_max_speed(max_speed);
          iols.attenuator->get_op_mode(mode);
          iols.attenuator->get_motor_state(state);
          iols.attenuator->get_acceleration(acceleration);
          iols.attenuator->get_deceleration(deceleration);
          iols.attenuator->get_resolution(resolution);
          iols.attenuator->get_current_idle(current_idle);
          iols.attenuator->get_current_move(current_move);
          spdlog::info("CURRENT SETTINGS :\n"
              "S/N          : {0}\n"
              "OFFSET       : {1}\n"
              "MAX SPEED    : {2}\n"
              "OP MODE      : {3}\n"
              "MOTOR STATE  : {4}\n"
              "ACCELERATION : {5}\n"
              "DELECERATION : {6}\n"
              "RESOLUTION   : {7}\n"
              "CURRENT IDLE : {8}\n"
              "CURRENT MOVE : {9}\n"
              ,sn,offset,max_speed,static_cast<int>(mode),static_cast<int>(state)
              ,acceleration,deceleration,resolution,current_idle,current_move);
        }
        else
        {
          spdlog::error("Unknown command {0}",argv[1]);
          print_help();
        }
        return 0;
      }
      else if (argc == 3)
      {
        if (std::string(argv[1]) == "move")
        {
          int32_t steps = std::strtol(argv[2],NULL,0);
          int32_t final_pos;
          spdlog::debug("Moving by {0} steps",steps);
          iols.attenuator->move(steps,final_pos,true);
          spdlog::info("Current position : {0}",final_pos);
        }
        else if (std::string(argv[1]) == "move_to")
        {
          int32_t set_pos = std::strtol(argv[2],NULL,0);
          int32_t final_pos;
          spdlog::debug("Moving to position {0}",set_pos);
          iols.attenuator->go(set_pos,final_pos,true);
          spdlog::info("Current position : {0}",final_pos);
        }
        else if (std::string(argv[1]) == "set_resolution")
        {
          uint16_t resolution = std::strtol(argv[2],NULL,0);
          spdlog::debug("Setting resolution to {0}",resolution);
          iols.attenuator->set_resolution(resolution);
        }
        else if (std::string(argv[1]) == "set_idle_current")
        {
          uint16_t ic = std::strtol(argv[2],NULL,0);
          spdlog::debug("Setting idle current to {0}",ic);
          iols.attenuator->set_idle_current(ic);
        }
        else if (std::string(argv[1]) == "set_move_current")
        {
          uint16_t mc = std::strtol(argv[2],NULL,0);
          spdlog::debug("Setting move current to {0}",mc);
          iols.attenuator->set_moving_current(mc);
        }
        else if (std::string(argv[1]) == "set_acceleration")
        {
          uint16_t acc = std::strtol(argv[2],NULL,0);
          spdlog::debug("Setting acceleration to {0}",acc);
          iols.attenuator->set_acceleration(acc);
        }
        else if (std::string(argv[1]) == "set_deceleration")
        {
          uint16_t dec = std::strtol(argv[2],NULL,0);
          spdlog::debug("Setting deceleration to {0}",dec);
          iols.attenuator->set_deceleration(dec);
        }
        else if (std::string(argv[1]) == "set_max_speed")
        {
          uint32_t ms = std::strtol(argv[2],NULL,0);
          spdlog::debug("Setting max speed to {0}",ms);
          iols.attenuator->set_max_speed(ms);
        }
        else
        {
          spdlog::error("Unknown command {0}",argv[1]);
          print_help();
        }
      }
      else
      {
        spdlog::error("Unknown attenuator command");
        print_help();
      }
      return 0;
    }
    else if(cmd == "power_meter")
    {
      if (g_ignore_pm)
      {
        spdlog::error("Power meter is being ignored.No operations possible");
        return 0;
      }
      if (iols.power_meter == nullptr)
      {
        spdlog::error("There is no open instance of the power meter");
        return 0;
      }
      spdlog::error("Power meter commands not yet implemented");
      return 0;
    }
    else
    {
      spdlog::error("Unknown command");
      return 0;
    }
  }
  catch(serial::PortNotOpenedException &e)
  {
    spdlog::critical("Port not open exception : {0}",e.what());
    return 1;
  }
  catch(serial::SerialException &e)
  {
    spdlog::critical("Serial exception : {0}",e.what());
    return 1;
  }
  catch(std::exception &e)
  {
    spdlog::critical("STL exception : {0}",e.what());
    return 1;
  }
  catch(...)
  {
    spdlog::critical("Caught an unexpected exception");
    return 1;
  }
  return 0;
}



int main(int argc, char** argv)
{
  // initiate spdlog
  spdlog::set_pattern("cib::serial : [%^%L%$] %v");
  spdlog::set_level(spdlog::level::trace); // Set global log level to info

  // set default values for the control variables
  g_ignore_laser = false;
  g_ignore_attenuator = false;
  g_ignore_pm = false;

  int c;
  opterr = 0;
  int report_level = SPDLOG_LEVEL_INFO;
  while ((c = getopt (argc, argv, "vi:")) != -1)
  {
    switch (c)
    {
      case 'v':
        if (report_level > 0)
        {
          report_level--;
        }
        break;
      case 'i':
      {
        if (std::string(optarg) == "laser")
        {
          spdlog::warn("Not operating the laser");
          g_ignore_laser = true;
        }
        else if (std::string(optarg) == "attenuator")
        {
          spdlog::warn("Not operating the attenuator");
          g_ignore_attenuator = true;
        }
        else if (std::string(optarg) == "power_meter")
        {
          spdlog::warn("Not operating the power meter");
          g_ignore_pm = true;
        }
        else
        {
          spdlog::error("Unknown argument {0}",optarg);
          return 0;
        }
        break;
      }
      default: /* ? */
        spdlog::warn("Usage: serial_manager [-v] [-i <device>]  \n(repeated -v flags further increase verbosity)\n device can be one of laser, attenuator, power_meter (repeated instances of the flag are accepted)");
        return 1;
    }
  }

  std::string level_name = spdlog::level::to_string_view(spdlog::get_level()).data();
  spdlog::info("Log level: {0} : {1}",static_cast<int>(spdlog::get_level()),level_name);
  spdlog::trace("Just testing a trace");
  spdlog::debug("Just testing a debug");


  // first enumerate the ports available
  spdlog::info("Enumerating the ports available with serial devices:");
  util::enumerate_ports();

  // map the devices
  int ret = 0;
  spdlog::info("Mapping the devices");
  iols.attenuator = nullptr;
  iols.laser = nullptr;
  iols.power_meter = nullptr;

  if (!g_ignore_attenuator)
  {
    iols.config.attenuator.serial_nr = ATT_SN;
    iols.config.attenuator.baud_rate = 38400;
  }
  if (!g_ignore_laser)
  {
    iols.config.laser.serial_nr = LASER_SN;
    iols.config.laser.baud_rate = 9600;
  }
  if (!g_ignore_pm)
  {
    iols.config.power_meter.serial_nr = PM_SN;
    iols.config.power_meter.baud_rate = 9600;
  }
  ret = map_devices();
  //ret = map_laser();
  if (ret != 0)
  {
    spdlog::critical("Failed to map devices. Clearing out.");
    unmap_devices();
    return 0;
  }

  // now start the real work
  // by default set to the appropriate settings
  print_help();

  // -- now start the real work
  char* buf;
  while ((buf = readline(">> ")) != nullptr)
  {
    if (strlen(buf) > 0) {
      add_history(buf);
    } else {
      free(buf);
      continue;
    }
    char *delim = (char*)" ";
    int count = 1;
    char *ptr = buf;
    while((ptr = strchr(ptr, delim[0])) != NULL) {
      count++;
      ptr++;
    }
    if (count > 0)
    {
      char **cmd = new char*[count];
      cmd[0] = strtok(buf, delim);
      int i;
      for (i = 1; cmd[i-1] != NULL && i < count; i++)
      {
        cmd[i] = strtok(NULL, delim);
      }
      if (cmd[i-1] == NULL) i--;
      int ret = run_command(i,cmd);
      delete [] cmd;
      if (ret == 255)
      {
        unmap_devices();
        return 0;
      }
      if (ret != 0)
      {
        spdlog::critical("Failed to run a command. Exiting.");
        unmap_devices();
        return ret;
      }
    }
    else
    {
      unmap_devices();
      return 0;
    }
    free(buf);
  }
  unmap_devices();
  return 0;
}

