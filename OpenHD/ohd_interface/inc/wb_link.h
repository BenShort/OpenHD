#ifndef STREAMS_H
#define STREAMS_H

#include <array>
#include <chrono>
#include <optional>
#include <utility>
#include <vector>

#include "../../lib/wifibroadcast/src/UdpWBReceiver.hpp"
#include "../../lib/wifibroadcast/src/UdpWBTransmitter.hpp"
#include "mavlink_settings/ISettingsComponent.hpp"
#include "openhd-action-handler.hpp"
#include "openhd-link-statistics.hpp"
#include "openhd-platform.hpp"
#include "openhd-profile.hpp"
#include "openhd-spdlog.hpp"
#include "openhd-telemetry-tx-rx.h"
#include "openhd-video-transmit-interface.h"
#include "wb_link_settings.hpp"
#include "wifi_card.hpp"
#include "ground_video_forwarder.h"

/**
 * This class takes a list of discovered wifi cards (and their settings) and
 * is responsible for configuring the given cards and then setting up all the Wifi-broadcast streams needed for OpenHD.
 * In the end, we have a link that has some broadcast characteristics for video (video is always broadcast from air to ground)
 * but also a bidirectional link (without re-transmission(s)) for telemetry.
 * This class assumes a corresponding instance on the air or ground unit, respective.
 */
class WBLink :public openhd::ITransmitVideo{
 public:
  /**
   * @param broadcast_cards list of discovered wifi card(s) that support monitor mode & are injection capable. Needs to be at least
   * one card, and only one card on an air unit. The given cards need to support monitor mode and either 2.4G or 5G wifi.
   * @param opt_action_handler global openhd action handler, optional (can be nullptr during testing of specific modules instead
   * of testing a complete running openhd instance)
   */
  explicit WBLink(OHDProfile profile,OHDPlatform platform,std::vector<std::shared_ptr<WifiCardHolder>> broadcast_cards,
                     std::shared_ptr<openhd::ActionHandler> opt_action_handler);
  WBLink(const WBLink&)=delete;
  WBLink(const WBLink&&)=delete;
  ~WBLink();
  // Verbose string about the current state.
  [[nodiscard]] std::string createDebug()const;
  // start or stop video data forwarding to another external device
  // NOTE: Only for the ground unit, and only for video (see OHDInterface for more info)
  void addExternalDeviceIpForwardingVideoOnly(const std::string& ip);
  void removeExternalDeviceIpForwardingVideoOnly(const std::string& ip);
  // returns all mavlink settings, values might change depending on the used hardware
  std::vector<openhd::Setting> get_all_settings();
  // This handle is used by ohd_telemetry to get / send telemetry (raw) data
  std::shared_ptr<openhd::TxRxTelemetry> get_telemetry_tx_rx_interface();
 private:
  // validate param, then schedule change
  bool request_set_frequency(int frequency);
  // validate param, then schedule change
  bool request_set_channel_width(int channel_width);
  // apply the frequency (wifi channel) of all wifibroadcast cards
  void apply_frequency_and_channel_width();
  // validate param, then schedule change
  bool request_set_txpower(int tx_power);
  // set the tx power of all wifibroadcast cards
  void apply_txpower();
  // validate param, then schedule change
  bool request_set_mcs_index(int mcs_index);
  // set the mcs index for all tx instances
  void apply_mcs_index();
  // These do not "break" the bidirectional connectivity and therefore
  // can be changed easily on the fly
  bool set_video_fec_block_length(int block_length);
  bool set_video_fec_percentage(int fec_percentage);
  bool set_enable_wb_video_variable_bitrate(int value);
  bool set_max_fec_block_size_for_platform(int value);
  // Check if all cards support changing the mcs index
  bool validate_cards_support_setting_mcs_index();
  // Check if all cards support changing the channel width
  bool validate_cards_support_setting_channel_width();
  // Make sure no processes interfering with monitor mode run on the given cards,
  // then sets them to monitor mode
  void takeover_cards_monitor_mode();
  // set the right frequency, channel width and tx power. Cards need to be in monitor mode already !
  void configure_cards();
  // start telemetry and video rx/tx stream(s)
  void configure_telemetry();
  void configure_video();
  std::unique_ptr<openhd::WBStreamsSettingsHolder> m_settings;
  // For telemetry, bidirectional in opposite directions
  std::unique_ptr<WBTransmitter> m_wb_tele_tx;
  std::unique_ptr<AsyncWBReceiver> m_wb_tele_rx;
  // For video, on air there are only tx instances, on ground there are only rx instances.
  std::vector<std::unique_ptr<WBTransmitter>> m_wb_video_tx_list;
  std::vector<std::unique_ptr<AsyncWBReceiver>> m_wb_video_rx_list;
  // Reads the current settings and creates the appropriate Radiotap Header params
  [[nodiscard]] RadiotapHeader::UserSelectableParams create_radiotap_params()const;
  [[nodiscard]] TOptions create_tx_options(uint8_t radio_port,bool enableFec)const;
  [[nodiscard]] ROptions create_rx_options(uint8_t radio_port)const;
  std::unique_ptr<WBTransmitter> create_wb_tx(uint8_t radio_port,bool enableFec);
  std::unique_ptr<AsyncWBReceiver> create_wb_rx(uint8_t radio_port,WBReceiver::OUTPUT_DATA_CALLBACK cb);
  [[nodiscard]] std::vector<std::string> get_rx_card_names()const;
 private:
  const OHDProfile m_profile;
  const OHDPlatform m_platform;
  std::vector<std::shared_ptr<WifiCardHolder>> m_broadcast_cards;
  std::shared_ptr<openhd::ActionHandler> m_opt_action_handler=nullptr;
  std::shared_ptr<spdlog::logger> m_console;
  // disable all openhd frequency checking - note that openhd just uses the proper iw command to set a frequency - if setting
  // the frequency actually had an effect, it doesn't know (cannot really know) and therefore QOpenHD can then report a different wifi freq,
  // even though the frequency actually hasn't changed
  static constexpr auto FIlE_DISABLE_ALL_FREQUENCY_CHECKS="/boot/openhd/disable_all_frequency_checks.txt";
  const bool m_disable_all_frequency_checks;
 private:
  // We have one worker thread for asynchronously performing operation(s) like changing the frequency
  // but also recalculating statistics that are then forwarded to openhd_telemetry for broadcast
  bool m_work_thread_run;
  std::unique_ptr<std::thread> m_work_thread;
  // Recalculate stats, apply settings asynchronously and more
  void loop_do_work();
  // update statistics, done in regular intervals, update data is given to the ohd_telemetry module via the action handler
  void update_statistics();
  static constexpr auto RECALCULATE_STATISTICS_INTERVAL=std::chrono::seconds(1);
  std::chrono::steady_clock::time_point m_last_stats_recalculation=std::chrono::steady_clock::now();
  // Do rate adjustments, does nothing if variable bitrate is disabled
  void perform_rate_adjustment();
  static constexpr auto RATE_ADJUSTMENT_INTERVAL=std::chrono::seconds(1);
  std::chrono::steady_clock::time_point m_last_rate_adjustment=std::chrono::steady_clock::now();
  int64_t last_tx_error_count=-1;
  int64_t last_recommended_bitrate=-1;
  // A bit dirty, some settings need to be applied asynchronous
  class WorkItem{
   public:
    explicit WorkItem(std::function<void()> work,std::chrono::steady_clock::time_point earliest_execution_time):
    m_earliest_execution_time(earliest_execution_time),m_work(std::move(work)){
    }
    void execute(){
      m_work();
    }
    bool ready_to_be_executed(){
      return std::chrono::steady_clock::now()>=m_earliest_execution_time;
    }
   private:
    const std::chrono::steady_clock::time_point m_earliest_execution_time;
    const std::function<void()> m_work;
  };
  void schedule_work_item(const std::shared_ptr<WorkItem>& work_item);
  // We limit changing specific params to one after another
  bool check_work_queue_empty();
  std::mutex m_work_item_queue_mutex;
  std::queue<std::shared_ptr<WorkItem>> m_work_item_queue;
  static constexpr auto DELAY_FOR_TRANSMIT_ACK =std::chrono::seconds(2);
 private:
  bool set_wb_rtl8812au_tx_pwr_idx_override(int value);
  bool has_rtl8812au();
 private:
  // Called by the camera stream on the air unit only
  // transmit video data via wifibradcast
  void transmit_video_data(int stream_index,const openhd::FragmentedVideoFrame& fragmented_video_frame) override;
  // Called by the wifibroadcast receiver on the ground unit only
  // Forward video data to the local udp port and/or external device(s) if they exist
  void forward_video_data(int stream_index,const uint8_t * data,int data_len);
 private:
  std::shared_ptr<openhd::TxRxTelemetry> m_tx_rx_handle;
  std::unique_ptr<GroundVideoForwarder> m_ground_video_forwarder;
 public:
  // Warning: This operation will block the calling thread for up to X ms.
  // During scan, you cannot change any wb settings
  struct ScanResult{
    bool success=false;
    uint32_t wifi_channel=0;
  };
  static constexpr std::chrono::seconds DEFAULT_SCAN_TIME{20};
  // checking both 2G and 5G channels would take too long
  ScanResult scan_channels(std::chrono::nanoseconds duration=DEFAULT_SCAN_TIME,bool check_2g_channels=false);
 private:
  std::atomic<bool> is_scanning=false;
  void reset_received_packets_count();
  int get_received_packets_count();
 private:
  // We return false on all the change settings request(s) if there is already a change operation queued
  // up or we currently perform a channel scan
  // Not completely "thread safe" so to say but good enough.
  bool check_in_state_support_changing_settings();
};

#endif
