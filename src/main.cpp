#include <c_online.h>

void setup() {
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, HIGH);

  Serial.begin(115200);
  while (!Serial) {}

  LittleFS.begin();
  Wire.begin();

  keep_log = LittleFS.exists("/log.txt");

  note("iDom Switch ." + String(version));
  offline = !LittleFS.exists("/online.txt");
  Serial.print(offline ? " OFFLINE" : " ONLINE");

  sprintf(host_name, "switch_%s", String(WiFi.macAddress()).c_str());
  WiFi.hostname(host_name);

  for (int i = 0; i < 2; i++) {
    pinMode(relay_pin[i], OUTPUT);
  }

  if (!readSettings(0)) {
    readSettings(1);
  }
  setLights("restore", false);

  if (RTCisrunning()) {
    start_time = RTC.now().unixtime() - offset - (dst ? 3600 : 0);
  }

  button1.setPushedCallback(&button1Single, (void*)"");
  button2.setPushedCallback(&button2Single, (void*)"");

  setupOTA();

  if (ssid != "" && password != "") {
    connectingToWifi();
  } else {
    initiatingWPS();
  }
}


bool readSettings(bool backup) {
  File file = LittleFS.open(backup ? "/backup.txt" : "/settings.txt", "r");
  if (!file) {
    note("The " + String(backup ? "backup" : "settings") + " file cannot be read");
    return false;
  }

  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, file.readString());

  if (json_object.isNull() || json_object.size() < 5) {
    note(String(backup ? "Backup" : "Settings") + " file error");
    file.close();
    return false;
  }

  file.seek(0);
  note("Reading the " + String(backup ? "backup" : "settings") + " file:\n " + file.readString());
  file.close();

  if (json_object.containsKey("ssid")) {
    ssid = json_object["ssid"].as<String>();
  }
  if (json_object.containsKey("password")) {
    password = json_object["password"].as<String>();
  }

  if (json_object.containsKey("smart")) {
    smart_string = json_object["smart"].as<String>();
    setSmart();
  }
  if (json_object.containsKey("uprisings")) {
    uprisings = json_object["uprisings"].as<int>() + 1;
  }
  if (json_object.containsKey("offset")) {
    offset = json_object["offset"].as<int>();
  }
  if (json_object.containsKey("dst")) {
    dst = json_object["dst"].as<bool>();
  }
  if (json_object.containsKey("restore")) {
    restore_on_power_loss = json_object["restore"].as<bool>();
  }
  if (json_object.containsKey("dawn_delay")) {
    dawn_delay = json_object["dawn_delay"].as<int>();
  }
  if (json_object.containsKey("dusk_delay")) {
    dusk_delay = json_object["dusk_delay"].as<int>();
  }

  if (restore_on_power_loss) {
    if (json_object.containsKey("light1")) {
      light1 = json_object["light1"].as<bool>();
    }
    if (json_object.containsKey("light2")) {
      light2 = json_object["light2"].as<bool>();
    }
  }

  if (json_object.containsKey("location")) {
    geo_location = json_object["location"].as<String>();
  }
  if (json_object.containsKey("sensors")) {
    also_sensors = json_object["sensors"].as<bool>();
  }

  saveSettings(false);

  return true;
}

void saveSettings() {
  saveSettings(true);
}

void saveSettings(bool log) {
  DynamicJsonDocument json_object(1024);

  json_object["ssid"] = ssid;
  json_object["password"] = password;

  json_object["smart"] = smart_string;
  json_object["uprisings"] = uprisings;
  json_object["offset"] = offset;
  json_object["dst"] = dst;
  json_object["restore"] = restore_on_power_loss;
  json_object["dusk_delay"] = dusk_delay;
  json_object["dawn_delay"] = dawn_delay;
  json_object["location"] = geo_location;
  json_object["sensors"] = also_sensors;

  json_object["light1"] = light1;
  json_object["light2"] = light2;

  if (writeObjectToFile("settings", json_object)) {
    if (log) {
      String logs;
      serializeJson(json_object, logs);
      note("Saving settings:\n " + logs);
    }

    writeObjectToFile("backup", json_object);
  } else {
    note("Saving the settings failed!");
  }
}


void sayHelloToTheServer() {
  // This function is only available with a ready-made iDom device.
}

void introductionToServer() {
  // This function is only available with a ready-made iDom device.
}

void startServices() {
  server.on("/hello", HTTP_POST, handshake);
  server.on("/set", HTTP_PUT, receivedOfflineData);
  server.on("/state", HTTP_GET, requestForState);
  server.on("/basicdata", HTTP_POST, exchangeOfBasicData);
  server.on("/log", HTTP_GET, requestForLogs);
  server.on("/log", HTTP_DELETE, clearTheLog);
  server.on("/admin/update", HTTP_POST, manualUpdate);
  server.on("/admin/log", HTTP_POST, activationTheLog);
  server.on("/admin/log", HTTP_DELETE, deactivationTheLog);
  server.begin();

  note(String(host_name) + (MDNS.begin(host_name) ? " started" : " unsuccessful!"));

  MDNS.addService("idom", "tcp", 8080);

  getTime();
  getOfflineData();
}

String getSwitchDetail() {
  return "";
    // This function is only available with a ready-made iDom device.
}

String getValue() {
  if (!light1 && !light2) {
    return "0";
  }

  return String(light1 ? "1" : "") + (light2 ? "2" : "");
}

void handshake() {
  if (server.hasArg("plain")) {
    readData(server.arg("plain"), true);
  }

  String reply = "\"id\":\"" + WiFi.macAddress()
  + "\",\"value\":" + getValue()
  + ",\"twilight\":" + twilight
  + ",\"cloudiness\":" + cloudiness
  + ",\"next_sunset\":" + next_sunset
  + ",\"next_sunrise\":" + next_sunrise
  + ",\"sun_check\":" + last_sun_check
  + ",\"restore\":" + restore_on_power_loss
  + ",\"dusk_delay\":" + dusk_delay
  + ",\"dawn_delay\":" + dawn_delay
  + ",\"location\":\"" + geo_location
  + "\",\"sensors\":" + also_sensors
  + ",\"version\":" + version
  + ",\"smart\":\"" + smart_string
  + "\",\"rtc\":" + RTCisrunning()
  + ",\"dst\":" + dst
  + ",\"offset\":" + offset
  + ",\"time\":" + (RTCisrunning() ? String(RTC.now().unixtime() - offset - (dst ? 3600 : 0)) : "0")
  + ",\"active\":" + String(start_time > 0 ? RTC.now().unixtime() - offset - (dst ? 3600 : 0) - start_time : 0)
  + ",\"uprisings\":" + uprisings
  + ",\"offline\":" + offline;

  Serial.print("\nHandshake");
  server.send(200, "text/plain", "{" + reply + "}");
}

void requestForState() {
  String reply = "\"state\":" + getValue();

  server.send(200, "text/plain", "{" + reply + "}");
}

void exchangeOfBasicData() {
  if (server.hasArg("plain")) {
    readData(server.arg("plain"), true);
  }

  String reply = "\"offset\":" + String(offset) + ",\"dst\":" + String(dst);

  if (RTCisrunning()) {
    reply += ",\"time\":" + String(RTC.now().unixtime() - offset - (dst ? 3600 : 0));
  }

  server.send(200, "text/plain", "{" + reply + "}");
}


void button1Single(void* s) {
  light1 = !light1;
  setLights("manual", true);
}

void button2Single(void* s) {
  light2 = !light2;
  setLights("manual", true);
}



void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(led_pin, LOW);
  } else {
    digitalWrite(led_pin, loop_time % 2 == 0);
    if (!sending_error) {
      note("Wi-Fi connection lost");
    }
    sending_error = true;
  }

  ArduinoOTA.handle();
  server.handleClient();
  MDNS.update();

  button1.poll();
  button2.poll();

  if (hasTimeChanged()) {
    getOnlineData();
    if (twilight_counter > 0) {
      if (--twilight_counter == 0) {
        automaticSettings(true);
        return;
      }
    }
    automaticSettings();
  }
}


bool hasTheLightChanged() {
  if (loop_time % 60 != 0 || geo_location.length() < 2 || !RTCisrunning()) {
    return false;
  }

  DateTime now = RTC.now();
  int current_time = (now.hour() * 60) + now.minute();
  bool result = false;

  if ((current_time > 51 && last_sun_check != now.day()) || next_sunset == -1 || next_sunrise == -1) {
    getSunriseSunset(now.day());
  }

  if (next_sunset > -1 && next_sunrise > -1) {
    if (current_time == next_sunset && !twilight) {
      twilight = true;
      result = true;
    }
    if (current_time == next_sunrise && twilight) {
      twilight = false;
      result = true;
    }
  }

  return result;
}

void readData(String payload, bool per_wifi) {
  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, payload);

  if (json_object.isNull()) {
    if (payload.length() > 0) {
      note("Parsing failed!");
    }
    return;
  }

  bool settings_change = false;
  bool details_change = false;
  String result = "";

  if (json_object.containsKey("offset")) {
    if (offset != json_object["offset"].as<int>()) {
      if (RTCisrunning() && !json_object.containsKey("time")) {
        RTC.adjust(DateTime((RTC.now().unixtime() - offset) + json_object["offset"].as<int>()));
        note("Time zone change");
      }

      offset = json_object["offset"].as<int>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("dst")) {
    if (dst != strContains(json_object["dst"].as<String>(), "1")) {
      dst = !dst;
      settings_change = true;

      if (RTCisrunning() && !json_object.containsKey("time")) {
        RTC.adjust(DateTime(RTC.now().unixtime() + (dst ? 3600 : -3600)));
        note(dst ? "Summer time" : "Winter time");
      }
    }
  }

  if (json_object.containsKey("time")) {
    uint32_t new_time = json_object["time"].as<uint32_t>() + offset + (dst ? 3600 : 0);
    if (new_time > 1546304461) {
      if (RTCisrunning()) {
        if (abs(new_time - RTC.now().unixtime()) > 60) {
          RTC.adjust(DateTime(new_time));
        }
      } else {
        RTC.adjust(DateTime(new_time));
        note("Adjust time");
        start_time = RTC.now().unixtime() - offset - (dst ? 3600 : 0);
        if (RTCisrunning() && !offline) {
          details_change = true;
        }
      }
    }
  }

  if (json_object.containsKey("smart")) {
    if (smart_string != json_object["smart"].as<String>()) {
      smart_string = json_object["smart"].as<String>();
      setSmart();
      if (per_wifi) {
        result += String(result.length() > 0 ? "&" : "") + "smart=" + getSmartString();
      }
      settings_change = true;
    }
  }

  if (json_object.containsKey("val")) {
    String newValue = json_object["val"].as<String>();
    if (getValue() != newValue) {
      light1 = strContains(newValue, "1") || strContains(newValue, "4");
      light2 = strContains(newValue, "2") || strContains(newValue, "4");
      setLights(per_wifi ? (json_object.containsKey("apk") ? "apk" : "local") : "cloud", false);
      if (per_wifi) {
        result += String(result.length() > 0 ? "&" : "") + "val=" + getValue();
      }
    }
  }

  if (json_object.containsKey("restore")) {
    if (restore_on_power_loss != strContains(json_object["restore"].as<String>(), "1")) {
      restore_on_power_loss = !restore_on_power_loss;
      details_change = true;
    }
  }

  if (json_object.containsKey("dusk_delay")) {
    if (dusk_delay != json_object["dusk_delay"].as<int>()) {
      if (next_sunset != -1) {
        next_sunset -= dusk_delay;
      }
      dusk_delay = json_object["dusk_delay"].as<int>();
      if (next_sunset != -1) {
        next_sunset += dusk_delay;
      }
      details_change = true;
    }
  }

  if (json_object.containsKey("dawn_delay")) {
    if (dawn_delay != json_object["dawn_delay"].as<int>()) {
      if (next_sunrise != -1) {
        next_sunrise -= dawn_delay;
      }
      dawn_delay = json_object["dawn_delay"].as<int>();
      if (next_sunset != -1) {
        next_sunrise += dawn_delay;
      }
      details_change = true;
    }
  }

  if (json_object.containsKey("location")) {
    if (geo_location != json_object["location"].as<String>()) {
      geo_location = json_object["location"].as<String>();
      getSunriseSunset(RTC.now().day());
      details_change = true;
    }
  }

  if (json_object.containsKey("sensors")) {
    if (also_sensors != strContains(json_object["sensors"].as<String>(), "1")) {
      also_sensors = !also_sensors;
      details_change = true;
    }
  }

  if (json_object.containsKey("light")) {
    if (((geo_location.length() < 2 || also_sensors) && twilight != strContains(json_object["light"].as<String>(), "t"))
    || (geo_location.length() > 2 && !also_sensors && cloudiness != strContains(json_object["light"].as<String>(), "t"))) {
      if (geo_location.length() < 2) {
        twilight = !twilight;
      } else {
        if (also_sensors) {
          twilight = !twilight;
        } else {
          cloudiness = !cloudiness;
        }
      }

      if (twilight && dusk_delay != 0) {
        twilight_counter = (dusk_delay * (dusk_delay < 0 ? -1 : 1)) * 60;
      } else {
          automaticSettings(true);
      }
    }
  }

  if (settings_change || details_change) {
    note("Received the data:\n " + payload);
    saveSettings();
  }
  if (!offline && (result.length() > 0 || details_change)) {
    if (details_change) {
      result += String(result.length() > 0 ? "&" : "") + "detail=" + getSwitchDetail();
    }
    putOnlineData(result);
  }
}

void setSmart() {
  if (smart_string.length() < 2) {
    smart_count = 0;
    return;
  }

  int count = 1;
  smart_count = 1;
  for (char b: smart_string) {
    if (b == ',') {
      count++;
    }
    if (b == smart_prefix) {
      smart_count++;
    }
  }

  if (smart_array != 0) {
    delete [] smart_array;
  }
  smart_array = new Smart[smart_count];
  smart_count = 0;

  String single_smart_string;

  for (int i = 0; i < count; i++) {
    single_smart_string = get1(smart_string, i);
    if (strContains(single_smart_string, String(smart_prefix))) {

      if (strContains(single_smart_string, "/")) {
        smart_array[smart_count].enabled = false;
        single_smart_string = single_smart_string.substring(1);
      } else {
        smart_array[smart_count].enabled = true;
      }

      if (strContains(single_smart_string, "_")) {
        smart_array[smart_count].on_time = single_smart_string.substring(0, single_smart_string.indexOf("_")).toInt();
        single_smart_string = single_smart_string.substring(single_smart_string.indexOf("_") + 1);
      } else {
        smart_array[smart_count].on_time = -1;
      }

      if (strContains(single_smart_string, "-")) {
        smart_array[smart_count].off_time = single_smart_string.substring(single_smart_string.indexOf("-") + 1).toInt();
        single_smart_string = single_smart_string.substring(0, single_smart_string.indexOf("-"));
      } else {
        smart_array[smart_count].off_time = -1;
      }

      if (strContains(single_smart_string, "4")) {
        smart_array[smart_count].lights = "12";
      } else {
        smart_array[smart_count].lights = strContains(single_smart_string, "1") ? "1" : "";
        smart_array[smart_count].lights += strContains(single_smart_string, "2") ? "2" : "";
        if (smart_array[smart_count].lights == "") {
          smart_array[smart_count].lights = "12";
        }
      }

      if (strContains(single_smart_string, "w")) {
        smart_array[smart_count].days = "w";
      } else {
        smart_array[smart_count].days = strContains(single_smart_string, "o") ? "o" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "u") ? "u" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "e") ? "e" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "h") ? "h" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "r") ? "r" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "a") ? "a" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "s") ? "s" : "";
      }

      smart_array[smart_count].on_at_night = strContains(single_smart_string, "n");
      smart_array[smart_count].off_at_day = strContains(single_smart_string, "d");
      smart_array[smart_count].on_at_night_and_time = strContains(single_smart_string, "n&");
      smart_array[smart_count].off_at_day_and_time = strContains(single_smart_string, "d&");
      smart_array[smart_count].react_to_cloudiness = strContains(single_smart_string, "z");
      smart_array[smart_count].access = 0;

      smart_count++;
    }
  }
  note("Smart contains " + String(smart_count) + " of " + String(smart_prefix));
}

bool automaticSettings() {
  return automaticSettings(hasTheLightChanged());
}

bool automaticSettings(bool light_changed) {
  bool result = false;
  DateTime now = RTC.now();
  String log = "Smart ";
  int current_time = -1;

  if (RTCisrunning()) {
    current_time = (now.hour() * 60) + now.minute();

    if (current_time == 120 || current_time == 180) {
      if (now.month() == 3 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 120 && !dst) {
        int new_time = now.unixtime() + 3600;
        RTC.adjust(DateTime(new_time));
        dst = true;
        note("Smart set to summer time");
        saveSettings();
        getSunriseSunset(now.day());
      }
      if (now.month() == 10 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 180 && dst) {
        int new_time = now.unixtime() - 3600;
        RTC.adjust(DateTime(new_time));
        dst = false;
        note("Smart set to winter time");
        saveSettings();
        getSunriseSunset(now.day());
      }
    }

    if (current_time == 61 && now.second() == 0) {
      checkForUpdate();
    }
  }

  int i = -1;
  while (++i < smart_count) {
    if (smart_array[i].enabled && (strContains(smart_array[i].days, "w") || (RTCisrunning() && strContains(smart_array[i].days, days_of_the_week[now.dayOfTheWeek()])))) {
      if (light_changed) {
        if (smart_array[i].on_at_night
        && (!smart_array[i].on_at_night_and_time || (smart_array[i].on_at_night_and_time && smart_array[i].on_at_night > -1 && smart_array[i].on_at_night < current_time) || (smart_array[i].react_to_cloudiness && cloudiness))
        && (twilight || (smart_array[i].react_to_cloudiness && cloudiness))) {
          if (strContains(smart_array[i].lights, "1")) {
            light1 = true;
          }
          if (strContains(smart_array[i].lights, "2")) {
            light2 = true;
          }
          result = true;
          log += "lowering at ";
          log += smart_array[i].react_to_cloudiness && cloudiness ? "cloudiness" : "dusk";
          log += smart_array[i].on_at_night_and_time && twilight ? " and time" : "";
        }
        if (smart_array[i].off_at_day
        && (!smart_array[i].off_at_day_and_time || (smart_array[i].off_at_day_and_time && smart_array[i].off_at_day > -1 && smart_array[i].off_at_day < current_time) || (smart_array[i].react_to_cloudiness && !cloudiness))
        && (!twilight || (smart_array[i].react_to_cloudiness && !cloudiness))) {
          if (strContains(smart_array[i].lights, "1")) {
            light1 = false;
          }
          if (strContains(smart_array[i].lights, "2")) {
            light2 = false;
          }
          result = true;
          log += "lifting at ";
          log += smart_array[i].react_to_cloudiness && !cloudiness ? "sunshine" : "dawn";
          log += smart_array[i].off_at_day_and_time && !twilight ? " and time" : "";
        }
      } else {
        if (RTCisrunning() && smart_array[i].access + 60 < now.unixtime()) {
          if (smart_array[i].on_time == current_time
          && (!smart_array[i].on_at_night_and_time || (smart_array[i].on_at_night_and_time && twilight))) {
            smart_array[i].access = now.unixtime();
            if (strContains(smart_array[i].lights, "1")) {
              light1 = true;
            }
            if (strContains(smart_array[i].lights, "2")) {
              light2 = true;
            }
            result = true;
            log += "on at time";
            log += smart_array[i].on_at_night_and_time ? " and dusk" : "";
          }
          if (smart_array[i].off_time == current_time
          && (!smart_array[i].off_at_day_and_time || (smart_array[i].off_at_day_and_time && !twilight))) {
            smart_array[i].access = now.unixtime();
            if (strContains(smart_array[i].lights, "1")) {
              light1 = false;
            }
            if (strContains(smart_array[i].lights, "2")) {
              light2 = false;
            }
            result = true;
            log += "off at time";
            log += smart_array[i].off_at_day_and_time ? " and dawn" : "";
          }
        }
      }
    }
  }

  if (result) {
    note(log);
    setLights("smart", true);
  } else {
    if (light_changed) {
      note("Smart didn't activate anything.");
    }
  }
  return result;
}

void setLights(String orderer, bool put_online) {
  String logs = "";
  if (digitalRead(relay_pin[0]) != light1) {
    logs += "\n 1 to " + String(light1);
  }
  digitalWrite(relay_pin[0], light1);

  if (digitalRead(relay_pin[1]) != light2) {
    logs += "\n 2 to " + String(light1);
  }
  digitalWrite(relay_pin[1], light2);

  if (logs.length() > 0) {
    note("Switch (" + orderer + "): " + logs);
    saveSettings();

    if (put_online) {
      putOnlineData("val=" + getValue());
    }
  }
}
