# Custom firmware for Yi camera based on MStar platform

This firmware is completely based on the work done by TheCrypt0
https://github.com/TheCrypt0/yi-hack-v4
It's a clone made for Yi cameras based on MStar platform.

I have no time to support the project, so feel free to clone/fork this git and modify it as you want.

## RTSP Server
I wrote a daemon that reads the video stream directly from the kernel driver memory and sends it to an application based on live555.
I was inspired by the following topic:
- @andy2301 - [Ideas for the RSTP rtsp and rtsp2301](https://github.com/xmflsct/yi-hack-1080p/issues/5#issuecomment-294326131)

The RTSP server code derives from live555 - http://www.live555.com/ and from the archive rtsp2303_srcbin_20170414-1630.zip posted in the link above.

## Table of Contents

- [Features](#features)
- [Supported cameras](#supported-cameras)
- [Getting started](#getting-started)
- [Build your own firmware](#build-your-own-firmware)
- [Unbricking](#unbricking)
- [Acknowledgments](#acknowledgments)
- [Disclaimer](#disclaimer)

## Features
This firmware contains the following features.
Apart from RTSP, snapshot and ONVIF, all the features are copied from the TheCrypt0 project.

- FEATURES
  - RTSP server - allows a RTSP stream of the video (high or low resolution) but without audio.
    - rtsp://IP-CAM/ch0_0.h264        (high res)
    - rtsp://IP-CAM/ch0_1.h264        (low res)
  - ONVIF server - standardized interfaces for IP cameras.
  - Snapshot service - allows to get a jpg with a web request.
  Gets the latest idr frame from the buffer and converts it to jpg (latest idr frame = no real time).
    - http://IP-CAM:8080/cgi-bin/snapshot.sh?res=low        (select resolution: low or high)
    - http://IP-CAM:8080/cgi-bin/snapshot.sh        (default high)
  - MQTT - Motion detection through mqtt protocol.
  - Web server - web configutation interface (port 8080).
  - SSH server - dropbear.
  - Telnet server - busybox.
  - FTP server.
  - Authentication for HTTP, RTSP and ONVIF server.
  - Proxychains-ng - Disabled by default. Useful if the camera is region locked.
  - Watermark removed.
  - The possibility to change some camera settings:
    - video saving mode
    - detection sensitivity
    - status led
    - ir led
    - rotate
  - Display of motion detect events and videos through a web page.
  - The possibility to disable all the cloud features.

## Supported cameras

Currently this project supports only the following cameras:

| Camera | Firmware | File prefix | Remarks |
| --- | --- | --- | --- |
| **Yi 1080p Home 4FUS** | 4.5.0* | y203c | - |
| **Yi 1080p Home 6FUS** | 4.5.0* | y203c | - |
| **Yi 1080p Home 9FUS** | 4.5.0* | y203c | - |
| **Yi 1080p Home BFUS** | 4.5.0* | y203c | - |
| **Yi 1080p Dome 6FUS** | 4.6.0* | h201c | Thanks to @skylarhays |
| **Yi 1080p Dome BFUS** | 4.6.0* | h201c | Thanks to @skylarhays |
| **Yi 1080p Home 9FUS** | 4.2.0* | y25 | Work in progress |
| **Yi 1080p Home 6FCN** | unknown | y203c | - |

This firmware completely overwrite the original firmware.
So, USE AT YOUR OWN RISK.

## Getting Started
1. Check that you have a correct Xiaomi Yi camera.

2. Get a microSD card, 16gb or less, and format it by selecting File System as FAT32.

3. Get the correct firmware files for your camera from the releases section (https://github.com/roleoroleo/yi-hack-6FUS_4.5.0/releases).

4. Save both files (for example home_y203c and sys_y203c) on root path of microSD card.

5. Remove power to the camera, insert the microSD card, turn the power back ON.

6. The yellow light will come ON and flash for roughly 30 seconds, which means the firmware is being flashed successfully. The camera will boot up.

7. The yellow light will come ON again for the final stage of flashing. This will take up to 2 minutes.

8. Blue light should come ON indicating that your WiFi connection has been successful (if not disable using app).

9. Go in the browser and access the web interface of the camera as a website (http://IP-CAM:8080). Find the IP address using your mobile app (Camera Settings --> Network Info --> IP Address). If the mobile app can't be paired, you may look for the IP on your router's portal (see connected devices).

10. Done.

## Build your own firmware
If you want to build your own firmware, clone this git and compile using a linux machine.
Quick explanation:
- Download and install the SDK for MStar platform: the file name is "MStar MSC3XX SDK.zip" (Google is your friend).
- Prepare the system installing all the necessary packages.
- Copy original home and rootfs partition files to ./stock_firmware/... (don't ask me where to find them)
- ./scripts/init_sysroot.all.sh
- ./scripts/compile.sh
- ./scripts/pack_fw.all.sh

## Unbricking
If your camera doesn't start repeat the hack as described above.
You can use the same procedure even if you have a backup copy of the original partitions.
If the bootloader works correctly the camera will restart.

## Acknowledgments
Special thanks to the following people.
- @TheCrypt0 - [https://github.com/TheCrypt0/yi-hack-v4](https://github.com/TheCrypt0/yi-hack-v4)
- @andy2301 - [Ideas for the RTSP](https://github.com/xmflsct/yi-hack-1080p/issues/5#issuecomment-298093437)
- All the people who worked on the previous projects "yi-hack".

---
### DISCLAIMER
**I AM NOT RESPONSIBLE FOR ANY USE OR DAMAGE THIS SOFTWARE MAY CAUSE. THIS IS INTENDED FOR EDUCATIONAL PURPOSES ONLY. USE AT YOUR OWN RISK.**
