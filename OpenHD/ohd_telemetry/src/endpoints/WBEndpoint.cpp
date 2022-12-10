//
// Created by consti10 on 09.12.22.
//

#include "WBEndpoint.h"

#include <utility>

WBEndpoint::WBEndpoint(std::shared_ptr<openhd::TxRxTelemetry> tx_rx_handle,std::string TAG)
    : MEndpoint(std::move(TAG)),
      m_tx_rx_handle(std::move(tx_rx_handle)){
  //assert(m_tx_rx_handle);
  if(!m_tx_rx_handle){
    openhd::log::get_default()->warn("WBEndpoint-tx rx handle is missing (no telemetry connection between air and ground)");
  }else{
    auto cb=[this](std::shared_ptr<std::vector<uint8_t>> data){
      MEndpoint::parseNewData(data->data(),data->size());
    };
    m_tx_rx_handle->register_on_receive_callback(cb);
  }
}

bool WBEndpoint::sendMessagesImpl(const std::vector<MavlinkMessage>& messages) {
  auto message_buffers= pack_messages(messages);
  for(const auto& message_buffer:message_buffers){
    auto shared=std::make_shared<std::vector<uint8_t>>(message_buffer);
    if(m_tx_rx_handle){
      m_tx_rx_handle->forward_to_send_data_cb(shared);
    }
  }
  return true;
}
