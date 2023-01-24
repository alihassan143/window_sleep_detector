// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter_window.h"
#include <wtsapi32.h>
#include <flutter/event_channel.h>
#include <flutter/event_sink.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/method_channel.h>
#include <flutter/standard_method_codec.h>
#include <windows.h>

#include <memory>
#include <optional>

#include "flutter/generated_plugin_registrant.h"

static constexpr int kBatteryError = -1;
static constexpr int kNoBattery = -2;
HANDLE hSleepEvent=NULL;
static int GetBatteryLevel() {
  SYSTEM_POWER_STATUS status;
  if (GetSystemPowerStatus(&status) == 0) {
    return kBatteryError;
  } else if (status.BatteryFlag == 128) {
    return kNoBattery;
  } else if (status.BatteryLifePercent == 255) {
    return kBatteryError;
  }
  return status.BatteryLifePercent;
}
bool IsSystemLocked() {
    HWND hwnd = GetForegroundWindow();
     DWORD pid; 
    GetWindowThreadProcessId(hwnd, &pid);
    if (hwnd == NULL) {
        return true;
    }else if (pid == GetCurrentProcessId()) {
        return false;
    }else{
    return true;
    }

    
  
    // event_sink_->Success(flutter::EncodableValue(
    //     status.ACLineStatus == 1 ? "charging" : "discharging"));
  
}
FlutterWindow::FlutterWindow(const flutter::DartProject& project)
    : project_(project) {}

FlutterWindow::~FlutterWindow() {
  if (power_notification_handle_) {
    UnregisterPowerSettingNotification(power_notification_handle_);
  }
}

bool FlutterWindow::OnCreate() {
  if (!Win32Window::OnCreate()) {
    return false;
  }

  RECT frame = GetClientArea();

  // The size here must match the window dimensions to avoid unnecessary surface
  // creation / destruction in the startup path.
  flutter_controller_ = std::make_unique<flutter::FlutterViewController>(
      frame.right - frame.left, frame.bottom - frame.top, project_);
  // Ensure that basic setup of the controller was successful.
  if (!flutter_controller_->engine() || !flutter_controller_->view()) {
    return false;
  }
  RegisterPlugins(flutter_controller_->engine());

  flutter::MethodChannel<> channel(
      flutter_controller_->engine()->messenger(), "samples.flutter.io/battery",
      &flutter::StandardMethodCodec::GetInstance());
  channel.SetMethodCallHandler(
      [](const flutter::MethodCall<>& call,
         std::unique_ptr<flutter::MethodResult<>> result) {
        if (call.method_name() == "getBatteryLevel") {
          int battery_level = GetBatteryLevel();

          if (battery_level == kBatteryError) {
            result->Error("UNAVAILABLE", "Battery level not available.");
          } else if (battery_level == kNoBattery) {
            result->Error("NO_BATTERY", "Device does not have a battery.");
          } else {
            result->Success(battery_level);
          }
        } if(call.method_name()=="getBatteryLevel2"){
          bool statusofscreen = IsSystemLocked();
          result->Success(statusofscreen);
        }
        
        else {
          result->NotImplemented();
        }
      });

  flutter::EventChannel<> charging_channel(
      flutter_controller_->engine()->messenger(), "samples.flutter.io/charging",
      &flutter::StandardMethodCodec::GetInstance());
  charging_channel.SetStreamHandler(
      std::make_unique<flutter::StreamHandlerFunctions<>>(
          [this](auto arguments, auto events) {
            this->OnStreamListen(std::move(events));
            return nullptr;
          },
          [this](auto arguments) {
            this->OnStreamCancel();
            return nullptr;
          }));

  SetChildContent(flutter_controller_->view()->GetNativeWindow());



  return true;
}

void FlutterWindow::OnDestroy() {
  if (flutter_controller_) {
    flutter_controller_ = nullptr;
  }

  Win32Window::OnDestroy();
}

LRESULT
FlutterWindow::MessageHandler(HWND hwnd, UINT const message,
                              WPARAM const wparam,
                              LPARAM const lparam) noexcept {

                                //  WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);
  // Give Flutter, including plugins, an opportunity to handle window messages.
  if (flutter_controller_) {
    std::optional<LRESULT> result =
        flutter_controller_->HandleTopLevelWindowProc(hwnd, message, wparam,
                                                      lparam);
                                                      
                                                     

    if (result) {
      return *result;
    }
  }

  switch (message) {
    case WM_FONTCHANGE:
      flutter_controller_->engine()->ReloadSystemFonts();
      break;
   case WM_POWERBROADCAST:
            if (wparam == PBT_POWERSETTINGCHANGE) {
                POWERBROADCAST_SETTING *setting = (POWERBROADCAST_SETTING *)lparam;
                if (IsEqualGUID(setting->PowerSetting, GUID_CONSOLE_DISPLAY_STATE)) {
                    DWORD consoleDisplayState = *(DWORD*)setting->Data;
                    if (consoleDisplayState == 0) {
                        event_sink_->Success(flutter::EncodableValue(
        "hibernate mode"));
                        // Display is off
                    } else if (consoleDisplayState == 1) {
                        event_sink_->Success(flutter::EncodableValue(
        "display mode"));
                        // Display is on
                    } else if (consoleDisplayState == 2) {
                       event_sink_->Success(flutter::EncodableValue(
        "dimm mode"));
                        // Display is dimmed
                    }
                }
            }
            break;

      case WM_WTSSESSION_CHANGE:
            switch (wparam) {
                case WTS_SESSION_LOCK:
                   event_sink_->Success(flutter::EncodableValue(
         "lock"));
                    break;
                case WTS_SESSION_UNLOCK:
                    // Windows is unlocked
                       event_sink_->Success(flutter::EncodableValue(
         "unlock"));
                    break;
            }
            break;
  }

  return Win32Window::MessageHandler(hwnd, message, wparam, lparam);
}

void FlutterWindow::OnStreamListen(
    std::unique_ptr<flutter::EventSink<>>&& events) {
  event_sink_ = std::move(events);
  // SendBatteryStateEvent();

  power_notification_handle_ =
      RegisterPowerSettingNotification(GetHandle(), &GUID_CONSOLE_DISPLAY_STATE, 0);
     
}
 

void FlutterWindow::OnStreamCancel() { event_sink_ = nullptr; }

void FlutterWindow::SendBatteryStateEvent() {
  SYSTEM_POWER_STATUS status;
  //  event_sink_->Success(flutter::EncodableValue(
  //       "simple mode"));
  if (GetSystemPowerStatus(&status) == 0 || status.ACLineStatus == 255) {
    event_sink_->Error("UNAVAILABLE", "Charging status unavailable");
    
  } else if(GetSystemPowerStatus(&status) == 1) {
     

    // event_sink_->Success(flutter::EncodableValue(
    //     status.ACLineStatus == 1 ? "charging" : "discharging"));
  }
    // if (status.BatteryFlag & (1 << 3))
    // {
    //       event_sink_->Success(flutter::EncodableValue(
    //     "sleep mode"));
    //     // laptop is in sleep mode
    // }
    // if (status.BatteryFlag & (1 << 4))
    // {
    //      event_sink_->Success(flutter::EncodableValue(
    //     "hibernate mode"));
    //     // laptop is in hibernate mode
    // }
 
//   SYSTEM_POWER_STATUS_EX status;
// if (GetSystemPowerStatusEx(&status, true)) {
//     if (status.SystemStatusFlag == 3) {
//       event_sink_->Success(flutter::EncodableValue("sleep mode"));
//     } else if (status.SystemStatusFlag == 4) {
//       event_sink_->Success(flutter::EncodableValue("hibernate mode"));
//     } else {
//          event_sink_->Success(flutter::EncodableValue("another mode"));
//     }
// } else {
//     event_sink_->Error("UNAVAILABLE", "Charging status unavailable");
// }
}


