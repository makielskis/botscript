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
  EXPECT_EQ("test_user", c.username());
  EXPECT_EQ("test_password", c.password());
  EXPECT_EQ("test_package", c.package());
  EXPECT_EQ("http://test.example.com", c.server());
  EXPECT_EQ("2.00", modules["base"]["wait_time_factor"]);
  EXPECT_EQ("b", modules["mod1"]["a"]);
  EXPECT_EQ("d", modules["mod1"]["c"]);
  EXPECT_EQ("f", modules["mod2"]["e"]);
  EXPECT_EQ("h", modules["mod2"]["g"]);
  EXPECT_EQ("0", modules["mod1"]["active"]);
  EXPECT_EQ("1", modules["mod2"]["active"]);
}

TEST(config_test, to_json_with_password_test) {
  config c0(TEST_CONFIG);
  config c(c0.to_json(true));

  std::map<std::string, config::string_map> modules = c.module_settings();
  EXPECT_EQ("test_user", c.username());
  EXPECT_EQ("test_password", c.password());
  EXPECT_EQ("test_package", c.package());
  EXPECT_EQ("http://test.example.com", c.server());
  EXPECT_EQ("2.00", modules["base"]["wait_time_factor"]);
  EXPECT_EQ("b", modules["mod1"]["a"]);
  EXPECT_EQ("d", modules["mod1"]["c"]);
  EXPECT_EQ("f", modules["mod2"]["e"]);
  EXPECT_EQ("h", modules["mod2"]["g"]);
  EXPECT_EQ("0", modules["mod1"]["active"]);
  EXPECT_EQ("1", modules["mod2"]["active"]);
}

TEST(config_test, to_json_without_password_test) {
  config c0(TEST_CONFIG);

  EXPECT_EQ(c0.to_json(false).find("test_password"), std::string::npos);
}

TEST(config_test, init_commands_test) {
  config c(TEST_CONFIG);

  config::command_sequence init_commands = c.init_command_sequence();

  ASSERT_EQ(8u, init_commands.size());

  EXPECT_EQ("base_set_wait_time_factor", init_commands[0].first);
  EXPECT_EQ("2.00", init_commands[0].second);

  EXPECT_EQ("base_set_proxy", init_commands[1].first);
  EXPECT_EQ("127.0.0.1:9000", init_commands[1].second);

  EXPECT_EQ("mod1_set_a", init_commands[2].first);
  EXPECT_EQ("b", init_commands[2].second);

  EXPECT_EQ("mod1_set_c", init_commands[3].first);
  EXPECT_EQ("d", init_commands[3].second);

  EXPECT_EQ("mod1_set_active", init_commands[4].first);
  EXPECT_EQ("0", init_commands[4].second);

  EXPECT_EQ("mod2_set_e", init_commands[5].first);
  EXPECT_EQ("f", init_commands[5].second);

  EXPECT_EQ("mod2_set_g", init_commands[6].first);
  EXPECT_EQ("h", init_commands[6].second);

  EXPECT_EQ("mod2_set_active", init_commands[7].first);
  EXPECT_EQ("1", init_commands[7].second);
}

TEST(config_test, value_of_test) {
  config c(TEST_CONFIG);

  EXPECT_EQ("2.00", c.value_of("base_wait_time_factor"));
  EXPECT_EQ("127.0.0.1:9000", c.value_of("base_proxy"));
  EXPECT_EQ("b", c.value_of("mod1_a"));
  EXPECT_EQ("d", c.value_of("mod1_c"));
  EXPECT_EQ("f", c.value_of("mod2_e"));
  EXPECT_EQ("h", c.value_of("mod2_g"));
  EXPECT_EQ("0", c.value_of("mod1_active"));
  EXPECT_EQ("1", c.value_of("mod2_active"));
}

TEST(config_test, set_test) {
  config c;

  c.set("base_test", "123");

  EXPECT_EQ("123", c.module_settings()["base"]["test"]);
}
