/*
 * PowerMeterSim.hh
 *
 *  Created on: Apr 3, 2024
 *      Author: Nuno Barros
 */

#ifndef LASERCONTROL_SRC_POWERMETERSIM_HH_
#define LASERCONTROL_SRC_POWERMETERSIM_HH_

#include <PowerMeter.hh>

namespace device
{

  /*
   *
   */
  class PowerMeterSim final : public PowerMeter
  {
  public:
    PowerMeterSim ();
    virtual ~PowerMeterSim ();
    PowerMeterSim (const PowerMeterSim &other) = delete;
    PowerMeterSim (PowerMeterSim &&other) = delete;
    PowerMeterSim& operator= (const PowerMeterSim &other) = delete;
    PowerMeterSim& operator= (PowerMeterSim &&other) = delete;
    // average window
    enum AQSetting {aQuery=0,aNone=1, aHalfSec=2,aOneSec=3,aThreeSecs=4,aTenSecs=5,aThirtySecs=6};
    // this is not true. Need the real power meter to know what are the values
    //enum PowerRange {Auto=-1,r30mW=0,r3mW=1,r300uW=2,r30uW=3,r3uW=4,r300nW=5,r30nW=6};
    enum BC20 {bQuery=0,bHold=1,bContinuous=2};

    enum  DiffuserSetting {dQuery=0,dOut=1,dIn=2,dNA=3};
    enum MainsFreq{mfQuery=0,mf50Hz=1,mf60Hz=2};
    // NOTE: Only the measurements valid for Vega instrument are implemented
    enum MeasurementMode{mmQuery=0,mmPassive=1,mmPower=2,mmEnergy=3,mmExposure=4};

    /**
     * Tell the meter what to feel for, in nm
     * @param wl
     */
    void set_wavelength(const uint32_t wl);
    void get_wavelength(uint32_t &wl);

    /**
     * Set the readout range of the meter
     * Resolution is from ~2% to 100% of the range
     * if you undercede or excede this, you won't trigger!
     *
     * Indices are "10J, 2J, 200mJ, 20mJ, 2mJ"
     *
     * @param range
     */
    void set_range(const int16_t range, std::string &answer);
    void get_range_fast(int16_t &range);

    /**
     * We're *trying* to set the Energy Threshold to the minimum value.
     * This is a function called Energy Treshold in the guide to programmers.
     * FIXME: This doesn't work! Fix it!
     *
     * @param et
     */
    void set_e_threshold(const uint32_t et);
    void get_e_threshold(uint32_t &et);

    void get_energy(double &energy);


    // low level commands
    // commands that are not obviously used
    //
    // AF
    void get_average_flag(bool &flag);

    // AQ
    void average_query(const AQSetting q, AQSetting &answer);
    void average_query(const uint16_t q, uint16_t &a);
    // AR
    // queries the available modes, fills the map and returns the current setting in the variable
    void get_all_ranges(int16_t &current_setting);
    // AW
    void get_all_wavelengths(uint16_t &current_setting);
    void get_all_wavelengths(std::string &answer);
    // BQ
    // BC20 Query
    void bc20_sensor_mode(BC20 query,BC20 &answer);
    // BT
    // BeamTrack
    // FIXME: not implemented
    void query_beam_track() {throw std::runtime_error("not implemented");}

    //DQ
    // Diffuser Query
    //FIXME: Do we need this?
    void diffuser_query(const DiffuserSetting s, DiffuserSetting &answer);

    // EE
    // Exposure Energy : reports up to date exposure energy measurement
    // if device not in energy mode, all returned values are 0
    void exposure_energy(double &energy, uint32_t &pulses, uint32_t &et);

    // EF
    // Energy Flag : reports whether there is a *new* energy reading
    void energy_flag(bool &new_val);

    // ER
    // Energy Ready : checks if device is ready for another energy reading
    void energy_ready(bool &new_val);

    // ER
    // Energy Threshold : Query and set the threshold setting
    // Note: do not use
    void energy_threshold(const uint32_t et, uint32_t &answer);

    //FE
    // Force energy measurement mode
    void force_energy(bool &success);

    //FP
    // Force power measurement mode
    void force_power(bool &success);

    //FQ
    // filter query
    void filter_query(const DiffuserSetting s, DiffuserSetting &answer);

    //FS
    // force screen : forces the instrument into one of the screens below
    void force_screen() { throw std::runtime_error("not implemented");}

    //FX
    // force exposure measurement mode
    void force_exposure(bool &success);

    // HC
    // head configuration : save settings
    void head_configuration() {throw std::runtime_error("not implemented");}

    // HI
    // head information : query sensor.
    ///FIXME: use this to identify the device
    /// NOTE: Use this to identify the hardware
    void head_info_raw(std::string &answer);
    void head_info(std::string &type, std::string &sn, std::string &name, bool &power, bool &energy, bool &freq);

    // HT
    // Report head type - return 2 letter code
    void head_type(std::string &type);

    // IC
    // Instrument configuration
    // write the configuration to memory
    void inst_config(bool &success);

    // II
    // Instrument information
    // query instrument for its information
    ///FIXME: Can also be used to identify the device
    void inst_info(std::string &id, std::string &sn, std::string &name);

    // KL
    // key legends
    void key_legends() {throw std::runtime_error("not implemented");}

    //MA
    // Mains frequency
    void mains_frequency(MainsFreq q, MainsFreq &answer);
    void mains_frequency(uint16_t q, uint16_t &answer);

    // MF
    // Queries the device for maximum pulse frequency at which the sensor can sample the
    // laser for energy measurements. Note that the response depends on the pulse width setting
    void max_freq(uint32_t &value);

    // MM
    // set the instrument to the specified measurement mode
    // option 0 is to query
    void measurement_mode(const MeasurementMode q, MeasurementMode &a);
    void measurement_mode(const uint16_t q, uint16_t &a);

    // PL
    // set the pulse length, this method should be called once to fill in the corresponding map
    void pulse_length(const uint16_t value, uint16_t &answer);

    // RE
    // reset the instrument
    // NOTE: This may cause the USB connection to be re-enumerated (and therefore, the port may change)
    void reset();

    // RN
    // Read Range : Reads the present active measurement range
    // NOTE: This should be the goto method to figure out the range setting
    void get_range(int32_t &range );

    // SE
    // Send Energy : queries the device for the *most recent* energy measurement
    // NOTE: To verify that the device
    // has not previously reported it to the user, this command should be used in together
    // with the EF command.
    void send_energy(double &value);


    // Wrapper method that combines EF and SE
    bool read_energy(double &energy);

    // SF
    // Send frequency : Queries device for frequency at which the laser is firing.
    void send_frequency(double &value);

    // SG
    // Send average : Requests most recent average calculated by the instrument.
    // NOTE: Use with $AF to ensure reading each average only once.
    // AF --> get_average_flag
    void send_average(double &value);

    // wrapper method that combines AF and SG
    bool read_average(double &value);

    // SI
    // send units : Queries meter for present measurement mode.
    // Returns a single character that represents the present measurement mode.
    void send_units(char &unit);
    // returns the long name of the unit
    void send_units_long(std::string &unit);

    //SK
    // simulate key press: Simulates pressing a key on the meter’s front panel
    // for Vega instrument: key in [0; 8]
    void simulate_key_press(uint16_t key) {throw std::runtime_error("not implemented");}

    // SP
    // send power: Queries device for the most recent Power Measurement
    // To verify that the device has not previously reported it to the user,
    // this command should be used in together with the EF command.
    // NOTE: use with EF
    void send_power(double &value);

    // SX
    // send max : Queries device for the maximum allowable reading on present scale.
    // Returns Max allowable reading for present range in scientific notation or AUTO if in
    // autoranging
    // -- if auto, returns 0.0
    void send_max(double &value);

    // TW
    // trigger window: Queries or sets the devices external trigger window size, in microseconds.
    // valid values: 0 or 1-50000
    // NOTE: This is part of the main methods
    void trigger_window(const uint16_t value, uint16_t &answer);

    // UT
    // user threshold : Queries and sets threshold for Pyroelectric and Photodiode energy sensors.
    // valid values: 0..2500 (units of 0.01%)
    // TODO: This is another of the main methods
    void user_threshold(const uint16_t value, uint16_t &answer);
    void query_user_threshold(uint16_t &current, uint16_t &min, uint16_t &max);

    // VE
    // version : Query device for version of embedded software
    void version(std::string &value);

    // WD
    // Wavelength add
    void wavelength_add(const uint16_t index, const uint16_t wl, bool &success) {throw std::runtime_error("not implemented");}
    // WE
    void wavelength_erase(const uint16_t index, bool &success) {throw std::runtime_error("not implemented");}

    // WI
    // Wavelength Index :
    // Set the device to work with wavelength at location <Index> in list of wavelengths.
    void wavelength_index(const uint16_t index, bool &success);

    // WL
    // wavelength: Set wavelength at presently active index to new value. <Wavelength> must be
    // between the lower and upper limits of the spectrum as returned in the AW command
    //TODO: this is a major command to be used
    void wavelength(const uint16_t wl, bool &success);

    // WN
    // Write range
    // Configure sensor to measure in a specific range
    // Note: The index of the highest numeric range is 0. The index of AUTO (when
    // applicable) is -1
    void write_range(const int16_t range, bool &success);

    // WW
    // Configure sensor to work with laser as defined in <wavelength-string> (must be a
    // string as returned by the AW command).
    void write_discrete_wavelength(const uint16_t index, bool &success) {throw std::runtime_error("not implemented");}


    /**
     * Custom methods that will be commonly used by a client
     */
    void get_range_map(std::map<int16_t,std::string> &r) {r = m_ranges;}
    //void set_measurement_mode(const uint16_t mode);
    void get_pulse_map(std::map<uint16_t,std::string> &r) {r = m_pulse_lengths;}
    void get_averages_map(std::map<uint16_t,std::string> &r) {r = m_ave_windows;}
    const std::pair<uint16_t, uint16_t> get_threshold_ranges() const {return m_threshold_ranges;}

  private:

//    void send_cmd(const std::string cmd, std::string &resp);
//    void init_pulse_lengths();

    MeasurementMode m_mmode;
    int16_t m_range;
    uint16_t m_wavelength;

    uint16_t m_e_threshold;

    AQSetting m_ave_query_state;
    uint16_t m_pulse_length;

    std::map<int16_t,std::string> m_ranges;
    //  std::map<int16_t,std::string> m_power_ranges;
    std::map<int16_t,std::string> m_wavelength_ranges;
    std::map<uint16_t,std::string> m_pulse_lengths;
    std::map<uint16_t,std::string> m_ave_windows;

    std::map<char,std::string> m_measurement_units;
    std::pair<uint16_t, uint16_t> m_threshold_ranges;


  };

} /* namespace device */

#endif /* LASERCONTROL_SRC_POWERMETERSIM_HH_ */
