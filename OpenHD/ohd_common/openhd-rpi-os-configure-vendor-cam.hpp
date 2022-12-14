//
// Created by consti10 on 24.10.22.
//

#ifndef OPENHD_OPENHD_OHD_COMMON_OPENHD_RPI_OS_CONFIGURE_VENDOR_CAM_HPP_
#define OPENHD_OPENHD_OHD_COMMON_OPENHD_RPI_OS_CONFIGURE_VENDOR_CAM_HPP_

#include "openhd-util.hpp"
#include "openhd-util-filesystem.hpp"
#include "openhd-spdlog.hpp"
#include "openhd-platform.hpp"
//for dynamic config
#include <fstream>
#include <iostream>
#include <dirent.h>
#include <cstring>

// Helper to reconfigure the rpi os for the different camera types
// This really is exhausting - some camera(s) are auto-detected, some are not,
// And also gst-rpicamsrc (mmal) and libcamera need different config.txt files.
// They are also slight differences between the RPI4/CM4 and RPI3 or older
// Currently we use Dynamic Config files, which only include the important lines, 
// the config.txt can be changed by the user again, just the dynamic part is overwritten by openhd
// Note that the action(s) here are required for the OS to detect and configure the camera -
// only a camera detected by the OS can then be detected by the OHD camera(s) discovery
namespace openhd::rpi::os{

enum class CamConfig {
  MMAL = 0, // raspivid / gst-rpicamsrc
  LIBCAMERA, // "normal" libcamera (autodetect)
  LIBCAMERA_IMX477, // "normal" libcamera, explicitly set to imx477 detection only
  LIBCAMERA_ARDUCAM, // pivariety libcamera (arducam special)
  LIBCAMERA_IMX519, // Arducam imx519 without autofocus
  VEYE_327, // Veye IMX290/IMX327 (older versions)
  VEYE_CSIMX307, // Veye IMX307
  VEYE_CSSC132, //Veye SC132
  VEYE_MVCAM, // Veye MV Cameras
  VEYE_CAM2M // Veye IMX327 (never versions), VEYE series with 200W resolution
};
static std::string cam_config_to_string(const CamConfig& cam_config){
  switch (cam_config) {
    case CamConfig::MMAL: return "mmal";
    case CamConfig::LIBCAMERA: return "libcamera";
    case CamConfig::LIBCAMERA_IMX477: return "libcamera_imx477";
    case CamConfig::LIBCAMERA_ARDUCAM: return "libcamera_arducam";
    case CamConfig::LIBCAMERA_IMX519: return "libcamera_imx519";
    case CamConfig::VEYE_327: return "veye_327";
    case CamConfig::VEYE_CSIMX307: return "veye_csimx307";
    case CamConfig::VEYE_CSSC132: return "veye_cssc132";
    case CamConfig::VEYE_MVCAM: return "veye_mvcam";
    case CamConfig::VEYE_CAM2M: return "veye_cam2m";

  }
  openhd::log::get_default()->warn("Error cam_config_to_string");
  assert(true);
  return "mmal";
}

static CamConfig cam_config_from_int(int val){
  if(val==0)return CamConfig::MMAL;
  if(val==1)return CamConfig::LIBCAMERA;
  if(val==2)return CamConfig::LIBCAMERA_IMX477;
  if(val==3)return CamConfig::LIBCAMERA_ARDUCAM;
  if(val==4)return CamConfig::LIBCAMERA_IMX519;
  if(val==5)return CamConfig::VEYE_327;
  if(val==6)return CamConfig::VEYE_CSIMX307;
  if(val==7)return CamConfig::VEYE_CSSC132;
  if(val==8)return CamConfig::VEYE_MVCAM;
  if(val==9)return CamConfig::VEYE_CAM2M;

  openhd::log::get_default()->warn("Error cam_config_from_int");
  assert(true);
  return CamConfig::MMAL;
}
static int cam_config_to_int(CamConfig cam_config){
  switch (cam_config) {
    case CamConfig::MMAL: return 0;
    case CamConfig::LIBCAMERA: return 1;
    case CamConfig::LIBCAMERA_IMX477: return 2;
    case CamConfig::LIBCAMERA_ARDUCAM: return 3;
    case CamConfig::LIBCAMERA_IMX519: return 4;
    case CamConfig::VEYE_327: return 5;
    case CamConfig::VEYE_CSIMX307: return 6;
    case CamConfig::VEYE_CSSC132: return 7;
    case CamConfig::VEYE_MVCAM: return 8;
    case CamConfig::VEYE_CAM2M: return 9;

  }
  openhd::log::get_default()->warn("Error cam_config_to_int");
  assert(true);
  return 0;
}

static bool validate_cam_config_settings_int(int val){
  return val>=0 && val<=9;
}

static constexpr auto CAM_CONFIG_FILENAME="/boot/openhd/curr_rpi_cam_config.txt";

static CamConfig get_current_cam_config_from_file(){
  OHDFilesystemUtil::create_directories("/boot/openhd/");
  if(!OHDFilesystemUtil::exists(CAM_CONFIG_FILENAME)){
    // The OHD image builder defaults to mmal, NOTE this is in contrast to the default rpi os release.
    OHDFilesystemUtil::write_file(CAM_CONFIG_FILENAME, std::to_string(cam_config_to_int(CamConfig::MMAL)));
    return CamConfig::MMAL;
  }
  auto content=OHDFilesystemUtil::read_file(CAM_CONFIG_FILENAME);
  auto content_as_int=std::stoi(content);
  return cam_config_from_int(content_as_int);
}

static void save_cam_config_to_file(CamConfig new_cam_config){
  OHDFilesystemUtil::create_directories("/boot/openhd/");
  OHDFilesystemUtil::write_file(CAM_CONFIG_FILENAME,std::to_string(cam_config_to_int(new_cam_config)));
}

static std::string get_file_name_for_cam_config(const OHDPlatform& platform,const CamConfig& cam_config){
  const bool is_rpi4=platform.board_type==BoardType::RaspberryPi4B || platform.board_type==BoardType::RaspberryPiCM4;
  std::string base_filename="/boot/openhd/configs/";
  if(cam_config==CamConfig::MMAL){
     cam_config.append("debug_rapha_");

    return base_filename;
  }else{
    if(is_rpi4){

      return base_filename;
    }else{

      return base_filename;
    }
  }
  assert(true);
  return "";
}

const auto rpi_config_file_path="/boot/config.txt";
// Applies the new cam config (rewrites the /boot/config.txt file)
// Then writes the type corresponding to the current configuration into the settings file.
static void apply_new_cam_config_and_save(const OHDPlatform& platform,CamConfig new_cam_config){
  openhd::log::get_default()->debug("Begin apply cam config "+ cam_config_to_string(new_cam_config));
  const auto filename= get_file_name_for_cam_config(platform,new_cam_config);
  if(!OHDFilesystemUtil::exists(filename.c_str())){
    openhd::log::get_default()->warn("Cannot apply new cam config, corresponding config.txt ["+filename+"] not found");
    return;
  }
  // move current config.txt to a backup file
  OHDUtil::run_command("mv",{rpi_config_file_path,"/boot/config_bup.txt"});
  // and copy over the new one
  OHDUtil::run_command("cp",{filename,rpi_config_file_path});
  // save the current selection (persistent setting)
  save_cam_config_to_file(new_cam_config);
  // Now we just need to reboot
  openhd::log::get_default()->debug("End apply cam config "+ cam_config_to_string(new_cam_config));
}

// Unfortunately complicated, since we need to perform the action asynchronously and then reboot
// but also have to make sure a eager user doesn't change the config multiple times and then the pi
// "reboots in between" a change
class ConfigChangeHandler{
 public:
  explicit ConfigChangeHandler(OHDPlatform platform): m_platform(platform){
    assert(m_platform.platform_type==PlatformType::RaspberryPi);
  }
  // Returns true if checks passed, false otherwise (param rejected)
  bool change_rpi_os_camera_configuration(int new_value_as_int){
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!validate_cam_config_settings_int(new_value_as_int)){
      // reject, not a valid value
      return false;
    }
    const auto current_configuration=get_current_cam_config_from_file();
    const auto new_configuration=cam_config_from_int(new_value_as_int);
    if(current_configuration==new_configuration){
      openhd::log::get_default()->warn("Not changing cam config,already at "+ cam_config_to_string(current_configuration));
      return true;
    }
    // this change requires a reboot, so only allow changing once at run time
    if(m_changed_once)return false;
    m_changed_once= true;
    // This will apply the changes asynchronous, even though we are "not done yet"
    // We assume nothing will fail on this command and return true already,such that we can
    // send the ack.
    apply_async(new_configuration);
    return true;
  }
 private:
  std::mutex m_mutex;
  bool m_changed_once=false;
  std::unique_ptr<std::thread> m_handle_thread;
  const OHDPlatform m_platform;
  void apply_async(CamConfig new_value){
    // This is okay, since we will restart anyways
    m_handle_thread=std::make_unique<std::thread>([new_value,this]{
      apply_new_cam_config_and_save(m_platform,new_value);
      std::this_thread::sleep_for(std::chrono::seconds(3));
      OHDUtil::run_command("systemctl",{"start", "reboot.target"});
    });
  }
};


}
#endif  // OPENHD_OPENHD_OHD_COMMON_OPENHD_RPI_OS_CONFIGURE_VENDOR_CAM_HPP_
