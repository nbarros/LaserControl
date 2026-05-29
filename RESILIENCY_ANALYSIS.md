# LaserControl Resiliency Analysis: Response Validation Issues

## Executive Summary

The LaserControl library has **significant resiliency gaps** in handling serial device responses. Response parsing lacks comprehensive validation, which could lead to:
- **Crashes** (array out-of-bounds, string index errors)
- **Silent failures** (accepting malformed responses)
- **Data corruption** (parsing wrong data as valid)

### Critical Categories
1. **Unchecked string indexing** (15+ instances)
2. **No response format validation** (10+ instances)
3. **Missing token count checks** (8+ instances)
4. **No response prefix validation** (6+ instances)
5. **Unsafe offset assumptions** (7+ instances)

---

## Detailed Issues by Class

### **PowerMeter Class** - Most Issues (~20 instances)

#### Issue 1: Unchecked String Indexing with Fixed Offsets

**Problem**: Direct access to string indices without bounds checking.

```cpp
// PowerMeter.cpp, line ~390 (bc20_sensor_mode)
try
{
  answer = static_cast<BC20>(std::stol(resp.substr(2,1)));  // ← UNSAFE!
}
catch (const std::exception& ex)
{
  util::throw_parse_error("PowerMeter::bc20_sensor_mode", ex);
}
```

**What could go wrong**:
- If `resp` is shorter than 3 characters, `substr(2,1)` throws `std::out_of_range`
- Response might be: `"*"` (1 char) or `"*AB"` (success but only 2 chars) → **crash**
- Error message doesn't indicate actual response length received

**Affected functions** (all with `resp.substr(X,Y)` without bounds):
- `bc20_sensor_mode()` - line ~390
- `mains_frequency()` - line ~862
- `diffuser_query()` - line ~422
- `average_query()` - line ~206
- `get_all_ranges()` - line ~268 (`resp = resp.substr(1)`)
- `get_all_wavelengths()` - line ~368 (`answer = answer.substr(1)`)
- `exposure_energy()` - accesses `tokens.at(1), tokens.at(2), tokens.at(3)` without checking size
- `measurement_mode()` - line ~912 (`resp.substr(1)`)
- `pulse_length()` - accesses tokens without size check
- Many more...

#### Issue 2: No Response Prefix Validation

**Problem**: PowerMeter protocol requires responses to start with `"*"` (success) or `"?"` (error), but many commands don't validate this.

```cpp
// PowerMeter.cpp, line ~164 (get_average_flag)
bool st = send_cmd(cmd, resp);
if (!st) { throw ...; }

// Directly parses without checking first character
try
{
  const std::string parsed = resp.substr(1, resp.size() - 3);  // ← No "*" check!
  flag = (std::stol(parsed)==0)?false:true;
}
```

**What could go wrong**:
- Device sends `"ERROR"` instead of `"*0"` → code tries to parse "RRO" as integer → **crash or wrong value**
- Device sends echo instead of response → parser gets garbage

**Functions missing prefix validation**:
- `force_energy()` - no "*" check
- `force_power()` - no "*" check  
- `force_exposure()` - no "*" check
- `get_range()` - no validation shown
- `measurement_mode()` - only checks after extracting value
- `wavelength()` - no prefix check

**Partial validation** (checks but after risky access):
- `set_range()` - checks `rr.at(0)=='*'` but no bounds check first
- `get_all_wavelengths()` - does `answer = answer.substr(1)` THEN checks

#### Issue 3: Missing Token Count Validation After Tokenization

**Problem**: After splitting response into tokens, code accesses tokens without checking count.

```cpp
// PowerMeter.cpp, line ~467 (exposure_energy)
std::vector<std::string> tokens;
util::tokenize_string(resp, tokens, " ");
// No size check here!

try
{
  const double parsed_energy = std::stod(tokens.at(1));  // ← crash if size < 2!
  const uint32_t parsed_pulses = std::stoul(tokens.at(2));  // ← crash if size < 3!
  const uint32_t parsed_et = std::stoul(tokens.at(3));  // ← crash if size < 4!
  energy = parsed_energy;
  pulses = parsed_pulses;
  et = parsed_et;
}
```

**What could go wrong**:
- Response: `"* 1.5"` (only 2 tokens) → accessing `tokens.at(2)` → **std::out_of_range exception**
- Response: `"*"` (only 1 token) → accessing `tokens.at(1)` → **crash**

**Functions with this pattern**:
- `exposure_energy()` - expects 4+ tokens, no size check
- `average_query()` - searches for "NONE" but doesn't validate token count before adding to map
- `get_all_ranges()` - removes first token but doesn't validate initial size
- `measurement_mode()` - accesses `tokens.at(0)` without checking
- `pulse_length()` - no token count validation
- `send_energy()` - calls stod() without format validation
- Multiple others

#### Issue 4: Unsafe offset-based parsing without bounds

```cpp
// PowerMeter.cpp, line ~268 (get_all_ranges)
resp = resp.substr(1);  // ← What if resp.size() < 1?
// ... later tokenizes and accesses ...
parsed_setting = std::stol(tokens.at(0)) & 0xFFFF;  // ← crash if empty
```

**Scenario**:
- Device sends only `"*\r\n"` → resp becomes empty after `substr(1)` → tokenize returns 0 tokens → `tokens.at(0)` crashes

#### Issue 5: Inconsistent Response Format Expectations

```cpp
// PowerMeter.cpp, line ~912 (measurement_mode)
if (q == 0)
{
  // Query case - expects specific format
  a = std::stoi(resp.substr(1));  // ← Assumes resp.size() >= 2
}
else 
{
  // Set case - response format differs
  if (resp.at(0) == '*')  // ← crashes if resp.empty()!
  {
    a = q;
  }
  else
  {
    measurement_mode(0,a);  // ← recursive call
  }
}
```

**Issues**:
- `resp.at(0)` without size check
- Recursive call on failed parsing (potential infinite recursion if both fail)
- Different response formats for query vs. set not documented

---

### **Laser Class** - 5+ Issues

#### Issue 1: Response Token Count Not Validated

```cpp
// Laser.cpp, line ~325 (security)
std::vector<std::string> lines;
read_lines(lines);

if (lines.size() == 0) { /* ... retry ... */ }
else if (lines.size() != 2)
{
  throw serial::IOException(..., "Failed to read security code. Got unexpected number of tokens : " + 
                            std::to_string(lines.size()));
}

// Expects exactly 2, but later code assumes this
resp = lines.at(1);  // ← Safe here because of check above ✓
```

**Better**, but still issue: If `lines.size() > 2`, gets extra data silently.

#### Issue 2: Undocumented Response Format

```cpp
// Laser.cpp, line ~327 (get_shot_count)
std::vector<std::string> lines;
read_lines(lines);

if (lines.size() == 0) { /* ... */ }
else if (lines.size() == 1)
{
  resp = lines.at(0);
  if (resp.size() > 0) { resp.erase(resp.size()-1); }  // ← Removes last char
}
else  // ← What if lines.size() > 2?
{
  resp = lines.at(1);  // ← Assumes lines.at(1) exists
  if (resp.size() > 0) { resp.erase(resp.size()-1); }
}

// Later, blindly converts
count = stoul(resp);  // ← What if resp contains non-digits?
```

**Issues**:
- Handles 0, 1, or 2+ tokens differently but doesn't validate max
- `stoul()` will throw on non-numeric input but error context lost
- Manual says "9 digit ASCII" but code doesn't validate length

#### Issue 3: Empty String Check Without Revalidation

```cpp
// Laser.cpp, line ~350 (security)
resp = lines.at(1);
if (resp.empty())  // ← Good check
{
  util::throw_parse_error("Laser::security", "empty response token");
}
resp.erase(resp.size()-1);  // ← Now erases last char (safe because of empty check)
```

This is handled well, but:
- What if `resp.size() < 2`? Erasing one char might leave garbage

---

### **Attenuator Class** - 3+ Issues

#### Issue 1: Fixed Token Position Without Count Validation

```cpp
// Attenuator.cpp, line ~453 (refresh_status)
std::vector<std::string> tokens;
util::tokenize_string(resp, tokens);

// Protocol expects 24+ tokens, but no minimum check!
try
{
  const enum OpMode op_mode = static_cast<enum OpMode>(std::stol(tokens.at(0)));  // ← crash if empty
  const enum MotorState motor_state = static_cast<enum MotorState>(std::stol(tokens.at(1)));
  // ... accessing tokens.at(2), .at(3), .at(4), .at(5), .at(6), .at(8), .at(9), .at(11), .at(12)
  // All without size check!
  const enum Resolution resolution = static_cast<enum Resolution>(std::stol(tokens.at(8)));  // ← crash if < 9
  // ...
}
```

**What could go wrong**:
- Device returns truncated status → fewer tokens → accessing `tokens.at(8)` when size=3 → **crash**
- No indication of actual vs. expected token count

#### Issue 2: Unsafe Substring Operations

```cpp
// Attenuator.cpp, line ~437 (refresh_status)
if (resp.size() < 2)
{
  util::throw_parse_error("Attenuator::refresh_status", "response too short");
}
resp = resp.substr(2);  // ← Safe because of check above ✓
```

**Good practice here**, but applies to only some functions.

#### Issue 3: Position Response Parsing

```cpp
// Attenuator.cpp, line ~504 (get_position)
std::vector<std::string> tokens;
util::tokenize_string(resp, tokens);

try
{
  if (tokens.at(0).size() != 1)  // ← crashes if size < 1!
  {
    throw std::runtime_error(...);
  }
  const int32_t parsed_position = std::stol(tokens.at(1));  // ← crashes if size < 2!
  const uint16_t parsed_status = std::stoul(tokens.at(0));
  // ...
}
```

**Issues**:
- No size check before accessing `tokens.at(0)`
- Checks size of token but not existence of token itself

---

### **Device Base Class** - Minor Issues

#### Issue 1: Response Suffix Stripping Assumes Format

```cpp
// Device.cpp, line ~178 (read_cmd)
if (answer.size() < m_response_suffix.size())
{
  std::string ta = util::rtrim(answer);
  answer = ta;
}
else
{
  answer.erase(answer.size() - m_response_suffix.size());  // ← Safe with check above
}
```

**Partial validation**: Checks size before erasing but doesn't validate that suffix actually matches.

**Scenario**:
- Expected suffix: `"\r\n"` (2 chars)
- Actual response: `"DATA\n\r"` (ends with wrong order)
- Code erases last 2 chars → removes correct data instead → **silent failure**

---

## Summary Table of Issues

| Class | Function | Issue Type | Line# | Severity |
|-------|----------|-----------|-------|----------|
| PowerMeter | bc20_sensor_mode | No bounds check, unchecked index | ~390 | **CRITICAL** |
| PowerMeter | mains_frequency | Unchecked substr(2,1) | ~862 | **CRITICAL** |
| PowerMeter | exposure_energy | No token count check | ~467 | **CRITICAL** |
| PowerMeter | get_all_ranges | substr(1) without bounds | ~268 | **CRITICAL** |
| PowerMeter | average_query | Unchecked substr, token access | ~206 | **CRITICAL** |
| PowerMeter | diffuser_query | Unchecked substr(2,1) | ~422 | **CRITICAL** |
| PowerMeter | measurement_mode | resp.at(0) without size check | ~912 | **CRITICAL** |
| PowerMeter | send_energy | stod() without validation | - | **HIGH** |
| PowerMeter | force_energy/power/exposure | No prefix validation | - | **HIGH** |
| PowerMeter | Multiple | No "*" success prefix validation | - | **HIGH** |
| Laser | get_shot_count | stoul() without format check | ~327 | **HIGH** |
| Laser | security | Undocumented format, stol() | ~325 | **HIGH** |
| Attenuator | refresh_status | No token count minimum | ~453 | **CRITICAL** |
| Attenuator | get_position | No size check before at(0) | ~504 | **CRITICAL** |
| Device | exchange_cmd | Suffix match not validated | ~178 | **MEDIUM** |

---

## Recommended Fixes

### Pattern 1: Always Validate Before String Access

**BEFORE:**
```cpp
answer = std::stoi(resp.substr(1));
```

**AFTER:**
```cpp
if (resp.size() < 2)
{
  throw serial::IOException(__FILE__, __LINE__, 
    std::string("Response too short: got ") + std::to_string(resp.size()) + 
    " chars, expected at least 2. Response: [" + util::escape(resp.c_str()) + "]");
}
answer = std::stoi(resp.substr(1));
```

### Pattern 2: Validate Token Count Before Indexing

**BEFORE:**
```cpp
std::vector<std::string> tokens;
util::tokenize_string(resp, tokens, " ");
value1 = stod(tokens.at(1));
value2 = stoul(tokens.at(2));
```

**AFTER:**
```cpp
std::vector<std::string> tokens;
util::tokenize_string(resp, tokens, " ");
if (tokens.size() < 3)
{
  throw serial::IOException(__FILE__, __LINE__, 
    std::string("Expected at least 3 tokens, got ") + std::to_string(tokens.size()) + 
    " from response: [" + util::escape(resp.c_str()) + "]");
}
value1 = stod(tokens.at(1));
value2 = stoul(tokens.at(2));
```

### Pattern 3: Validate Response Prefix (for PowerMeter)

**BEFORE:**
```cpp
bool st = send_cmd(cmd.str(), resp);
if (!st) throw ...;
// Directly use resp
answer = std::stol(resp.substr(1));
```

**AFTER:**
```cpp
bool st = send_cmd(cmd.str(), resp);
if (!st) throw ...;

if (resp.empty())
{
  throw serial::IOException(__FILE__, __LINE__, "Empty response from command: " + cmd.str());
}
if (resp.at(0) != '*')
{
  throw serial::IOException(__FILE__, __LINE__, 
    std::string("Command failed or unexpected response. Expected '*' prefix, got: [") + 
    util::escape(resp.c_str()) + "]");
}
if (resp.size() < 2)
{
  throw serial::IOException(__FILE__, __LINE__, 
    std::string("Response too short: ") + util::escape(resp.c_str()));
}
answer = std::stol(resp.substr(1));
```

### Pattern 4: Create Helper for Safe Substring Extraction

```cpp
// In utilities.hh
inline std::string safe_substr(const std::string& str, size_t pos, size_t len, 
                               const std::string& context)
{
  if (str.size() < pos + len)
  {
    throw std::runtime_error(
      context + ": attempted substr(" + std::to_string(pos) + "," + std::to_string(len) + 
      ") on string of size " + std::to_string(str.size()) + ": [" + 
      util::escape(str.c_str()) + "]");
  }
  return str.substr(pos, len);
}

// Usage
auto value = std::stoi(safe_substr(resp, 1, 1, "PowerMeter::bc20_sensor_mode"));
```

---

## Impact Assessment

### Risk Level: **HIGH**

**Consequences**:
- **Crashes**: Potential std::out_of_range or std::stoi exceptions
- **Silent corruption**: Accepting malformed responses as valid
- **Difficult debugging**: Error messages don't indicate actual vs. expected format
- **Field failures**: Intermittent failures depending on device response timing/format

**Likelihood**:
- Medium/High: Devices may send truncated responses on timeout
- Low latency networks might scramble packets
- Device firmware bugs could send unexpected formats

---

## Files to Review

Primary source files with issues:
1. `/home/nbarros/work/dune/cib/sw/LaserControl/src/PowerMeter.cpp` - **20+ instances**
2. `/home/nbarros/work/dune/cib/sw/LaserControl/src/Laser.cpp` - **5+ instances**
3. `/home/nbarros/work/dune/cib/sw/LaserControl/src/Attenuator.cpp` - **3+ instances**
4. `/home/nbarros/work/dune/cib/sw/LaserControl/src/Device.cpp` - **1-2 instances**

Header validation in:
- `/home/nbarros/work/dune/cib/sw/LaserControl/include/PowerMeter.hh` (protocol docs)
- `/home/nbarros/work/dune/cib/sw/LaserControl/include/Laser.hh` (protocol docs)

---

## Next Steps

1. **Priority 1**: Fix PowerMeter class (most instances, most critical)
2. **Priority 2**: Fix Laser and Attenuator classes
3. **Priority 3**: Create helper functions to prevent future issues
4. **Priority 4**: Add unit tests for malformed response handling
