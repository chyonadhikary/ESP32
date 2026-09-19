# ESP32 + SSD1315 OLED বাংলা Lyrics Web Server

এটি classic ESP32 38-pin এবং 0.96-inch 128x64 SSD1315 I2C OLED-এর জন্য। OLED wiring: `GND->GND`, `VCC->3V3`, `SCL->GPIO22`, `SDA->GPIO21`।

## কীভাবে কাজ করে

ESP32 boot হলে `ESP32-Bangla-OLED` নামে একটি Wi-Fi hotspot এবং web server চালু করে। Hotspot password: `12345678`। ফোন দিয়ে hotspot-এ যুক্ত হয়ে browser-এ `http://192.168.4.1` খুলুন। সেখানে নিজের router-এর SSID/password পাঠালে ESP32 একই সঙ্গে router-এও যুক্ত হবে। router থেকে IP পাওয়া গেলে সেই IP OLED-এ দেখাবে; phone-টি router Wi-Fi-তে যুক্ত করে ওই IP-তে page খুলুন।

Web page-এ সম্পূর্ণ SRT paste অথবা `.srt` file নির্বাচন করা যায়। Browser SRT timestamp parse করে, বাংলা text-কে canvas দিয়ে 128x64 monochrome bitmap বানায়, তারপর binary payload ESP32-তে পাঠায়। ফলে browser-এ বাংলা যুক্তাক্ষর ও কার সাধারণত সঠিকভাবে shape হয় এবং ESP32-তে ভারী Bengali shaping engine লাগে না।

## ব্যবহারিক ধাপ

1. Build ও flash করে ESP32 চালু করুন।
2. Phone Wi-Fi থেকে `ESP32-Bangla-OLED`-এ যুক্ত হন; password `12345678`।
3. Browser-এ `http://192.168.4.1` খুলুন। captive portal না খুললে এই URL নিজে লিখুন।
4. নিজের router-এর SSID/password দিয়ে **Wi-Fi-তে যুক্ত করুন** চাপুন।
5. OLED-এ নতুন router IP দেখুন। Phone-কে একই router-এ যুক্ত করে `http://নতুন-IP` খুলুন।
6. SRT file নির্বাচন করুন অথবা সম্পূর্ণ SRT paste করুন।
7. **SRT convert করে OLED-এ পাঠান** চাপুন।
8. Success message আসার পর গানটি `0:00` থেকে চালান। Upload button চাপার মুহূর্তকে lyric `0:00` ধরা হয়; তাই গানটি আগে pause করে রেখে upload চাপার সঙ্গে সঙ্গে Play করলে sync ভালো হবে।

## Wi-Fi ভুলে যাওয়া

Web page-এর **Wi-Fi password ভুলে যাও / Forget** button saved SSID/password মুছে দেয় এবং ESP32 setup hotspot চালু রাখে। নিরাপত্তার জন্য বাস্তব ব্যবহারে hotspot password ও authentication আরও শক্ত করা উচিত।

## Build

```powershell
idf.py fullclean
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

`COM3`-এর জায়গায় Device Manager-এ CP2102-এর আসল COM port দিন। Build সফল হওয়ার পরে `.elf` তৈরি হবে; তারপর flash করুন।

এই project-টি ESP-IDF v6.1-এর জন্য প্রস্তুত করা হয়েছে। ESP-IDF v6.1-এ GPIO ও I2C driver component আলাদা হওয়ায় `main/CMakeLists.txt`-এ `esp_driver_gpio` এবং `esp_driver_i2c` dependency আছে। I2C-এর জন্য নতুন `driver/i2c_master.h` API ব্যবহার করা হয়েছে। Project পরিবর্তন বা অন্য branch থেকে copy করার পরে প্রথমবার `idf.py fullclean` চালিয়ে তারপর build করুন।

## সীমা ও সতর্কতা

- এই version-এ SRT bitmap RAM-এ থাকে; ESP32 reset হলে uploaded lyrics মুছে যাবে, কিন্তু Wi-Fi credentials NVS-এ থাকে। Reset-এর পরে SRT আবার upload করতে হবে।
- সর্বোচ্চ 64টি SRT entry এবং মোট payload প্রায় 66 KB। সাধারণ ছোট/মাঝারি song SRT-এর জন্য যথেষ্ট।
- Browser canvas-এ এক entry সর্বোচ্চ তিন লাইনে wrap হয়; অতিরিক্ত text কেটে যেতে পারে।
- ESP32 নিজে audio বাজায় না; গান অন্য device-এ চালাতে হবে।
- SRT paste করার পরে upload শুরু হলে playback clock শুরু হয়।
- Wi-Fi setup page একই সঙ্গে AP এবং router connection চালায়; OLED-এ router IP দেখানোর পর phone-কে router network-এ বদলাতে হবে।
