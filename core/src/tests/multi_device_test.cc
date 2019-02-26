/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "gtest/gtest.h"
#include "include/bareos.h"
#include "lib/parse_conf.h"
#include "stored/stored_conf.h"
#include "stored/stored_globals.h"

typedef std::unique_ptr<ConfigurationParser> PConfigParser;

static void InitGlobals() { storagedaemon::my_config = nullptr; }

static storagedaemon::DeviceResource* GetMultiDeviceResource(
    ConfigurationParser& my_config)
{
  CommonResourceHeader* p = nullptr;
  while ((p = my_config.GetNextRes(storagedaemon::R_DEVICE, p))) {
    storagedaemon::DeviceResource* device =
        reinterpret_cast<storagedaemon::DeviceResource*>(p);
    if (device->multi_devices_count) { return device; }
  }
  return nullptr;
}

TEST(Storagedaemon, MultiDeviceTest_ConfigParameter)
{
  InitGlobals();
  std::string path_to_config =
      PROJECT_SOURCE_DIR "/src/tests/configs/stored_multi_device/";

  PConfigParser my_config(
      storagedaemon::InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = my_config.get();

  ASSERT_TRUE(my_config->ParseConfig());
  auto device = GetMultiDeviceResource(*my_config);
  ASSERT_TRUE(device);

  EXPECT_EQ(device->multi_devices_count, 3);
}

static uint32_t CountAllDeviceResources(ConfigurationParser& my_config)
{
  uint32_t count = 0;
  CommonResourceHeader* p = nullptr;
  while ((p = my_config.GetNextRes(storagedaemon::R_DEVICE, p))) { count++; }
  return count;
}

TEST(Storagedaemon, MultiDeviceTest_CountAllAutomaticallyCreatedResources)
{
  InitGlobals();
  std::string path_to_config =
      PROJECT_SOURCE_DIR "/src/tests/configs/stored_multi_device/";

  PConfigParser my_config(
      storagedaemon::InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = my_config.get();

  ASSERT_TRUE(my_config->ParseConfig());
  auto count = CountAllDeviceResources(*my_config);

  EXPECT_EQ(count, 3);
}

static uint32_t CheckNamesOfAllThreeConfiguredDeviceResources(
    ConfigurationParser& my_config)
{
  uint32_t count_str_ok = 0;
  uint32_t count_devices = 0;
  CommonResourceHeader* p = nullptr;

  while ((p = my_config.GetNextRes(storagedaemon::R_DEVICE, p))) {
    storagedaemon::DeviceResource* device =
        reinterpret_cast<storagedaemon::DeviceResource*>(p);
    const char* str = nullptr;
    ++count_devices;
    switch (count_devices) {
      case 1:
        str = "MultiDeviceResource___1";
        break;
      case 2:
        str = "MultiDeviceResource___2";
        break;
      case 3:
        str = "MultiDeviceResource___3";
        break;
      default:
        return 0;
    }
    std::string name_of_device(device->name());
    std::string name_to_compare(str ? str : "???");
    if (name_of_device == name_to_compare) { ++count_str_ok; }
  }
  return count_str_ok;
}

TEST(Storagedaemon, MultiDeviceTest_CheckNamesOfAutomaticallyCreatedResources)
{
  InitGlobals();
  std::string path_to_config =
      PROJECT_SOURCE_DIR "/src/tests/configs/stored_multi_device/";

  PConfigParser my_config(
      storagedaemon::InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = my_config.get();

  ASSERT_TRUE(my_config->ParseConfig());

  auto count = CheckNamesOfAllThreeConfiguredDeviceResources(*my_config);

  EXPECT_EQ(count, 3);
}
