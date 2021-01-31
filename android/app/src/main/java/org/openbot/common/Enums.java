package org.openbot.common;

public class Enums {
  public enum LogMode {
    ALL_IMGS,
    CROP_IMG,
    PREVIEW_IMG,
    ONLY_SENSORS
  }

  public enum ControlMode {
    GAMEPAD,
    PHONE,
    MQTT
  }

  public enum SpeedMode {
    TURTLE,
    SLOW,
    NORMAL,
    FAST
  }

  public enum DriveMode {
    DUAL,
    GAME,
    JOYSTICK
  }
}
