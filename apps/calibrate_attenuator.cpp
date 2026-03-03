/*
 * calibrate_attenuator.cpp
 *
 *  Created on: May 31, 2024
 *      Author: Nuno Barros
 */

#include <Attenuator.hh>
#include <PowerMeter.hh>
#include <Laser.hh>
#include <utilities.hh>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using device::PowerMeter;
using device::Laser;
using device::Attenuator;
using json = nlohmann::json;

namespace
{

struct ScanPoint
{
  int32_t position;
  std::vector<double> samples;
  double mean;
  double stddev;
  double uncertainty;
  double normalized;
  double normalized_uncertainty;
};

struct FitResult
{
  double offset;
  double objective;
};

double stepsToTrans(int32_t pos, double offset = 0.0)
{
  return std::pow(std::cos((M_PI / 180.0) * ((static_cast<double>(pos) - (offset + 3900.0)) / 43.333)), 2);
}

double transToSteps(double trans, double offset = 0.0)
{
  if (trans < 0.0 || trans > 1.0)
  {
    throw std::range_error("transToSteps expects transmission in [0,1]");
  }

  const double step = -43.333 * 180.0 / M_PI * std::acos(std::sqrt(trans)) + (offset + 3900.0);
  return step;
}

void sample_laser(PowerMeter& meter,
                  uint32_t num_samples,
                  uint32_t max_tries,
                  uint32_t sample_period_ms,
                  std::vector<double>& samples)
{
  samples.clear();
  uint32_t tries = 0;

  while (samples.size() < num_samples && tries < max_tries)
  {
    double tmp = 0.0;
    if (meter.read_energy(tmp))
    {
      samples.push_back(tmp);
      std::this_thread::sleep_for(std::chrono::milliseconds(sample_period_ms));
    }
    ++tries;
  }

  if (samples.size() < num_samples)
  {
    throw std::runtime_error("Could not collect requested number of energy samples from power meter");
  }
}

double compute_mean(const std::vector<double>& values)
{
  if (values.empty())
  {
    return 0.0;
  }

  double sum = 0.0;
  for (double value : values)
  {
    sum += value;
  }
  return sum / static_cast<double>(values.size());
}

double compute_stddev(const std::vector<double>& values, double mean)
{
  if (values.size() < 2)
  {
    return 0.0;
  }

  double sumsq = 0.0;
  for (double value : values)
  {
    const double diff = value - mean;
    sumsq += diff * diff;
  }
  return std::sqrt(sumsq / static_cast<double>(values.size() - 1));
}

void configure_laser(Laser& laser, const json& cfg)
{
  if (cfg.contains("prescale"))
  {
    laser.set_prescale(cfg.at("prescale").get<uint32_t>());
  }
  if (cfg.contains("pump_voltage_kv"))
  {
    laser.set_pump_voltage(cfg.at("pump_voltage_kv").get<float>());
  }
  if (cfg.contains("qswitch_us"))
  {
    laser.set_qswitch(cfg.at("qswitch_us").get<uint32_t>());
  }
  if (cfg.contains("repetition_rate_hz"))
  {
    laser.set_repetition_rate(cfg.at("repetition_rate_hz").get<float>());
  }

  const bool open_shutter = cfg.value("open_shutter", true);
  if (open_shutter)
  {
    laser.shutter_open();
  }
  else
  {
    laser.shutter_close();
  }
}

void configure_powermeter(PowerMeter& meter, const json& cfg)
{
  if (cfg.contains("wavelength_nm"))
  {
    meter.set_wavelength(cfg.at("wavelength_nm").get<uint32_t>());
  }

  if (cfg.contains("range"))
  {
    bool ok = false;
    meter.set_range(cfg.at("range").get<int16_t>(), ok);
    if (!ok)
    {
      throw std::runtime_error("Failed to configure power meter range");
    }
  }

  if (cfg.contains("energy_threshold"))
  {
    uint16_t answer = 0;
    meter.user_threshold(cfg.at("energy_threshold").get<uint16_t>(), answer);
  }

  bool forced = false;
  meter.force_energy(forced);
  if (!forced)
  {
    throw std::runtime_error("Failed to switch power meter to energy mode");
  }
}

void configure_attenuator(Attenuator& attenuator, const json& cfg)
{
  if (cfg.contains("resolution"))
  {
    attenuator.set_resolution(cfg.at("resolution").get<uint16_t>());
  }
  if (cfg.contains("max_speed"))
  {
    attenuator.set_max_speed(cfg.at("max_speed").get<uint32_t>());
  }
  if (cfg.contains("idle_current"))
  {
    attenuator.set_idle_current(cfg.at("idle_current").get<uint16_t>());
  }
  if (cfg.contains("moving_current"))
  {
    attenuator.set_moving_current(cfg.at("moving_current").get<uint16_t>());
  }
  if (cfg.contains("acceleration"))
  {
    attenuator.set_acceleration(cfg.at("acceleration").get<uint16_t>());
  }
  if (cfg.contains("deceleration"))
  {
    attenuator.set_deceleration(cfg.at("deceleration").get<uint16_t>());
  }
}

FitResult fit_offset(const std::vector<ScanPoint>& points,
                     double offset_min,
                     double offset_max,
                     double offset_step)
{
  if (points.empty())
  {
    throw std::runtime_error("No points available for fit");
  }
  if (offset_step <= 0.0)
  {
    throw std::range_error("offset_step must be > 0");
  }

  FitResult best;
  best.offset = offset_min;
  best.objective = std::numeric_limits<double>::infinity();

  for (double offset = offset_min; offset <= offset_max; offset += offset_step)
  {
    double objective = 0.0;
    for (const auto& point : points)
    {
      const double model = stepsToTrans(point.position, offset);
      const double sigma = (point.normalized_uncertainty > 0.0) ? point.normalized_uncertainty : 1e-6;
      const double residual = point.normalized - model;
      objective += (residual * residual) / (sigma * sigma);
    }

    if (objective < best.objective)
    {
      best.objective = objective;
      best.offset = offset;
    }
  }

  return best;
}

void dump_results_csv(const std::string& output,
                      const std::vector<ScanPoint>& points,
                      const FitResult& fit)
{
  std::ofstream out(output.c_str());
  if (!out.is_open())
  {
    throw std::runtime_error("Could not open output file: " + output);
  }

  out << "position,mean_energy,stddev,uncertainty,normalized,normalized_uncertainty,model_transmission,residual\n";
  out << std::setprecision(10);
  for (const auto& point : points)
  {
    const double model = stepsToTrans(point.position, fit.offset);
    out << point.position << ","
        << point.mean << ","
        << point.stddev << ","
        << point.uncertainty << ","
        << point.normalized << ","
        << point.normalized_uncertainty << ","
        << model << ","
        << (point.normalized - model) << "\n";
  }
}

json load_config(const std::string& path)
{
  std::ifstream in(path.c_str());
  if (!in.is_open())
  {
    throw std::runtime_error("Could not open config file: " + path);
  }

  json cfg;
  in >> cfg;
  return cfg;
}

std::string resolve_port(const json& dev_cfg)
{
  const std::string port = dev_cfg.at("port").get<std::string>();
  if (port != "auto")
  {
    return port;
  }

  if (!dev_cfg.contains("serial_number"))
  {
    throw std::runtime_error("port is 'auto' but serial_number is missing");
  }

  const std::string serial = dev_cfg.at("serial_number").get<std::string>();
  const std::string found = util::find_port(serial);
  if (found.empty())
  {
    throw std::runtime_error("Could not auto-resolve serial port for serial number: " + serial);
  }
  return found;
}

void print_usage(const char* prog)
{
  std::cout << "Usage: " << prog << " <calibration_config.json>\n";
}

}

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    print_usage(argv[0]);
    return 1;
  }

  try
  {
    const json cfg = load_config(argv[1]);

    const json& laser_cfg = cfg.at("laser");
    const json& pm_cfg = cfg.at("power_meter");
    const json& att_cfg = cfg.at("attenuator");
    const json& scan_cfg = cfg.at("scan");

    const std::string laser_port = resolve_port(laser_cfg);
    const std::string pm_port = resolve_port(pm_cfg);
    const std::string att_port = resolve_port(att_cfg);

    const uint32_t laser_baud = laser_cfg.value("baud", 9600u);
    const uint32_t pm_baud = pm_cfg.value("baud", 9600u);
    const uint32_t att_baud = att_cfg.value("baud", 38400u);

    Laser laser(laser_port.c_str(), laser_baud);
    PowerMeter meter(pm_port.c_str(), pm_baud);
    Attenuator attenuator(att_port.c_str(), att_baud);

    if (laser_cfg.contains("nominal"))
    {
      configure_laser(laser, laser_cfg.at("nominal"));
    }
    if (pm_cfg.contains("nominal"))
    {
      configure_powermeter(meter, pm_cfg.at("nominal"));
    }
    if (att_cfg.contains("nominal"))
    {
      configure_attenuator(attenuator, att_cfg.at("nominal"));
    }

    const int32_t start_position = scan_cfg.at("start_position").get<int32_t>();
    const int32_t max_position = scan_cfg.at("max_position").get<int32_t>();
    const int32_t step_size = scan_cfg.at("step_size").get<int32_t>();
    const uint32_t shots_per_position = scan_cfg.value("shots_per_position", 50u);
    const uint32_t max_read_tries = scan_cfg.value("max_read_tries", shots_per_position * 30u);
    const uint32_t settle_ms = scan_cfg.value("settle_ms", 200u);
    const uint32_t sample_period_ms = scan_cfg.value("sample_period_ms", 100u);

    if (step_size <= 0)
    {
      throw std::range_error("scan.step_size must be > 0");
    }
    if (max_position < start_position)
    {
      throw std::range_error("scan.max_position must be >= scan.start_position");
    }

    const json fit_cfg = cfg.value("fit", json::object());
    const double offset_min = fit_cfg.value("offset_min", -5000.0);
    const double offset_max = fit_cfg.value("offset_max", 5000.0);
    const double offset_step = fit_cfg.value("offset_step", 1.0);

    const json out_cfg = cfg.value("output", json::object());
    const std::string output_csv = out_cfg.value("results_csv", std::string("attenuation_calibration.csv"));

    std::cout << "Starting scan from position " << start_position
              << " to " << max_position
              << " with step " << step_size
              << " and " << shots_per_position << " shots per point" << std::endl;

    laser.fire_start();

    std::vector<ScanPoint> points;
    for (int32_t position = start_position; position <= max_position; position += step_size)
    {
      int32_t reached = 0;
      attenuator.go(position, reached, true);
      std::this_thread::sleep_for(std::chrono::milliseconds(settle_ms));

      std::vector<double> samples;
      sample_laser(meter, shots_per_position, max_read_tries, sample_period_ms, samples);

      const double mean = compute_mean(samples);
      const double stddev = compute_stddev(samples, mean);
      const double uncertainty = (samples.size() > 1) ? (stddev / std::sqrt(static_cast<double>(samples.size()))) : 0.0;

      ScanPoint point;
      point.position = reached;
      point.samples = samples;
      point.mean = mean;
      point.stddev = stddev;
      point.uncertainty = uncertainty;
      point.normalized = 0.0;
      point.normalized_uncertainty = 0.0;
      points.push_back(point);

      std::cout << "Position " << reached
                << " -> mean energy = " << mean
                << " +- " << uncertainty
                << " (N=" << samples.size() << ")" << std::endl;
    }

    laser.fire_stop();

    if (points.empty())
    {
      throw std::runtime_error("No scan points collected");
    }

    double reference = 0.0;
    for (const auto& point : points)
    {
      if (point.mean > reference)
      {
        reference = point.mean;
      }
    }

    if (reference <= 0.0)
    {
      throw std::runtime_error("Invalid reference energy for normalization");
    }

    for (auto& point : points)
    {
      point.normalized = point.mean / reference;
      point.normalized_uncertainty = point.uncertainty / reference;
    }

    const FitResult fit = fit_offset(points, offset_min, offset_max, offset_step);
    dump_results_csv(output_csv, points, fit);

    std::cout << "Calibration complete." << std::endl;
    std::cout << "Best-fit offset = " << fit.offset << std::endl;
    std::cout << "Objective (weighted SSE) = " << fit.objective << std::endl;
    std::cout << "Results written to " << output_csv << std::endl;
    std::cout << "Example: transmission at start position from fit = "
              << stepsToTrans(start_position, fit.offset)
              << ", inverse check steps(T=0.5) = "
              << transToSteps(0.5, fit.offset) << std::endl;

    return 0;
  }
  catch (const std::exception& ex)
  {
    std::cerr << "Calibration failed: " << ex.what() << std::endl;
    return 2;
  }
}


