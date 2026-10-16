#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/MultiSerialInterface.h>
#include <Arduino.h>

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif

#include "NodePrefs.h"
#include "MyMesh.h"

enum class UIEventType {
    none,
    contactMessage,
    channelMessage,
    roomMessage,
    newContactMessage,
    ack
};

class AbstractUITask : public MyMesh::Listener {
protected:
  mesh::MainBoard* _board;
  MultiSerialInterface* _interfaceManager;

  AbstractUITask(mesh::MainBoard* board, MultiSerialInterface* interfaceManager) : _board(board), _interfaceManager(interfaceManager) {
  }

public:
  bool hasConnection() const { return _interfaceManager->isConnected(); }
  uint16_t getBattMilliVolts() const { return _board->getBattMilliVolts(); }
  bool isBluetoothEnabled() const { return _interfaceManager->isBluetoothEnabled(); }
  void enableBluetooth() { _interfaceManager->enableBluetooth(); }
  void disableBluetooth() { _interfaceManager->disableBluetooth(); }

  virtual void notify(UIEventType t = UIEventType::none) = 0;
#ifdef ENABLE_HOST_DISPLAY
  virtual size_t peekHostButton(uint8_t* frame) { return 0; }
  virtual void consumeHostButton() {}
  virtual size_t hostDisplayCommand(const uint8_t* req, size_t len, uint8_t* resp) { return 0; }
#endif
#ifdef ENABLE_POSITION_BUTTON
  void onLocalSendQueued() override { notify(UIEventType::ack); }
#endif
  virtual void loop() = 0;
};
