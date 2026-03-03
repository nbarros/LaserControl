#include <utilities.hh>

#include <cstdint>
#include <exception>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

void expect_true(bool condition, const std::string& message)
{
  if (!condition)
  {
    throw std::runtime_error(message);
  }
}

void test_trim_helpers()
{
  expect_true(util::ltrim("  abc\n") == "abc\n", "ltrim should remove leading whitespace");
  expect_true(util::rtrim("\tabc  ") == "\tabc", "rtrim should remove trailing whitespace");
  expect_true(util::trim(" \tabc\r\n") == "abc", "trim should remove leading and trailing whitespace");
  expect_true(util::trim(" \n\r\t") == "", "trim of all-whitespace string should be empty");
}

void test_tokenize_string_default_separator()
{
  std::string value = "alpha;beta;gamma";
  std::vector<std::string> tokens;
  util::tokenize_string(value, tokens);

  expect_true(tokens.size() == 3, "tokenize_string should split by ';'");
  expect_true(tokens[0] == "alpha", "first token mismatch");
  expect_true(tokens[1] == "beta", "second token mismatch");
  expect_true(tokens[2] == "gamma", "third token mismatch");
  expect_true(value == "gamma", "tokenize_string should mutate input to last token");
}

void test_tokenize_string_custom_separator()
{
  std::string value = "one::two::three";
  std::vector<std::string> tokens;
  util::tokenize_string(value, tokens, "::");

  expect_true(tokens.size() == 3, "custom separator token count mismatch");
  expect_true(tokens[0] == "one", "custom token 0 mismatch");
  expect_true(tokens[1] == "two", "custom token 1 mismatch");
  expect_true(tokens[2] == "three", "custom token 2 mismatch");
}

void test_escape_overloads()
{
  const std::string raw = "line1\nline2\tend\r";
  const std::string expected = "line1\\nline2\\tend\\r";

  expect_true(util::escape(raw) == expected, "escape(std::string) mismatch");
  expect_true(util::escape(raw.c_str()) == expected, "escape(const char*) mismatch");

  const std::string custom = util::escape("a,b;c", std::set<char>{',', ';'}, '!');
  expect_true(custom == "a!,b!;c", "escape with custom marker/set mismatch");
}

void test_char2int()
{
  expect_true(util::char2int('0') == 0, "char2int('0') mismatch");
  expect_true(util::char2int('7') == 7, "char2int('7') mismatch");
  expect_true(util::char2int('9') == 9, "char2int('9') mismatch");
}

void test_serialize_map_uint16()
{
  std::map<uint16_t, std::string> values;
  values[1] = "one";
  values[20] = "twenty";

  const std::string serialized = util::serialize_map(values);
  expect_true(serialized == "{\"1\":\"one\",\"20\":\"twenty\"}", "serialize_map(uint16_t) mismatch");
}

void test_serialize_map_int16()
{
  std::map<int16_t, std::string> values;
  values[-1] = "minus";
  values[2] = "plus";

  const std::string serialized = util::serialize_map(values);
  expect_true(serialized == "{\"-1\":\"minus\",\"2\":\"plus\"}", "serialize_map(int16_t) mismatch");
}

void test_throw_parse_error()
{
  bool got_exception = false;
  try
  {
    util::throw_parse_error("MethodA", "bad format");
  }
  catch (const std::runtime_error& ex)
  {
    got_exception = true;
    expect_true(std::string(ex.what()) == "MethodA parse error: bad format", "throw_parse_error(method, detail) message mismatch");
  }

  expect_true(got_exception, "throw_parse_error(method, detail) should throw");

  got_exception = false;
  try
  {
    try
    {
      throw std::logic_error("nested failure");
    }
    catch (const std::exception& inner)
    {
      util::throw_parse_error("MethodB", inner);
    }
  }
  catch (const std::runtime_error& ex)
  {
    got_exception = true;
    expect_true(std::string(ex.what()) == "MethodB parse error: nested failure", "throw_parse_error(method, exception) message mismatch");
  }

  expect_true(got_exception, "throw_parse_error(method, exception) should throw");
}

}

int main()
{
  try
  {
    test_trim_helpers();
    test_tokenize_string_default_separator();
    test_tokenize_string_custom_separator();
    test_escape_overloads();
    test_char2int();
    test_serialize_map_uint16();
    test_serialize_map_int16();
    test_throw_parse_error();

    std::cout << "All LaserControl utility tests passed" << std::endl;
    return 0;
  }
  catch (const std::exception& ex)
  {
    std::cerr << "Test failure: " << ex.what() << std::endl;
    return 1;
  }
}
