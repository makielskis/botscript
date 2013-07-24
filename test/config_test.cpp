#include "gtest/gtest.h"

#include <string>

#include "../src/config.h"

using namespace std;
using namespace botscript;

#define TEST_CONFIG \
  "{"\
  "	\"username\": \"test_user\","\
  "	\"password\": \"test_password\","\
  "	\"package\": \"test_package\","\
  "	\"server\": \"http://test.example.com\","\
  "	\"modules\": {"\
  "		\"mod1\": {"\
  "			\"active\": \"0\","\
  "			\"a\": \"b\","\
  "			\"c\": \"d\""\
  "		},"\
  "		\"mod2\": {"\
  "			\"active\": \"1\","\
  "			\"e\": \"f\","\
  "			\"g\": \"h\""\
  "		},"\
  "		\"base\": {"\
  "			\"wait_time_factor\": \"2.00\","\
  "			\"proxy\": \"127.0.0.1:9000\""\
  "		}"\
  "	}"\
  "}"

TEST(config_test, from_json_test) {
  config c(TEST_CONFIG);

  std::map<std::string, config::string_map> modules = c.module_settings();
  ASSERT_EQ("test_user", c.username());
  ASSERT_EQ("test_password", c.password());
  ASSERT_EQ("test_package", c.package());
  ASSERT_EQ("http://test.example.com", c.server());
  ASSERT_EQ("2.00", modules["base"]["wait_time_factor"]);
  ASSERT_EQ("b", modules["mod1"]["a"]);
  ASSERT_EQ("d", modules["mod1"]["c"]);
  ASSERT_EQ("f", modules["mod2"]["e"]);
  ASSERT_EQ("h", modules["mod2"]["g"]);
  ASSERT_EQ("0", modules["mod1"]["active"]);
  ASSERT_EQ("1", modules["mod2"]["active"]);
}

TEST(config_test, to_json_with_password_test) {
  config c0(TEST_CONFIG);
  config c(c0.to_json(true));

  std::map<std::string, config::string_map> modules = c.module_settings();
  ASSERT_EQ("test_user", c.username());
  ASSERT_EQ("test_password", c.password());
  ASSERT_EQ("test_package", c.package());
  ASSERT_EQ("http://test.example.com", c.server());
  ASSERT_EQ("2.00", modules["base"]["wait_time_factor"]);
  ASSERT_EQ("b", modules["mod1"]["a"]);
  ASSERT_EQ("d", modules["mod1"]["c"]);
  ASSERT_EQ("f", modules["mod2"]["e"]);
  ASSERT_EQ("h", modules["mod2"]["g"]);
  ASSERT_EQ("0", modules["mod1"]["active"]);
  ASSERT_EQ("1", modules["mod2"]["active"]);
}

TEST(config_test, to_json_without_password_test) {
  config c0(TEST_CONFIG);

  ASSERT_EQ(c0.to_json(false).find("test_password"), std::string::npos);
}
