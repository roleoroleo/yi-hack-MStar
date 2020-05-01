# DO NOT FLASH THIS FIRMWARE! THESE ARE EXPERIMENTAL FEATURES.
# Please check the original Roleo repo for stable builds.


# Custom firmware for Yi camera based on MStar platform

This firmware is completely based on the work done by TheCrypt0
https://github.com/TheCrypt0/yi-hack-v4
It's a clone made for Yi cameras based on MStar platform.

**But you can't choose between TheCrypt0 and MStar: if your cam is based on Hi3518e Chipset then you have to use TheCrypt0, if your cam is based on MStar Infinity Chipset then you have to use MStar.**

I have no time to support the project, so feel free to clone/fork this git and modify it as you want.

## RTSP Server
I wrote a daemon that reads the video stream directly from the kernel driver memory and sends it to an application based on live555.
I was inspired by the following topic:
- @andy2301 - [Ideas for the RSTP rtsp and rtsp2301](https://github.com/xmflsct/yi-hack-1080p/issues/5#issuecomment-294326131)

The RTSP server code derives from live555 - http://www.live555.com/ and from the archive rtsp2303_srcbin_20170414-1630.zip posted in the link above.

There is a known problem with ffmpeg, see https://github.com/roleoroleo/yi-hack-MStar/issues/36 for details.

### RTSP audio support (many thanks to @PieVo for adding it):
The datapath of the audio is as follows:
Mic -> ADC -> Kernel sound driver -> TinyAlsa lib -> OMX ALSA plugin -> Camera application (rmm)

To maintain audio support for the original Yi application, the audio should be cloned at one of the steps with the following in mind:
- Kernel driver can be used by 1 "sink"
- TinyAlsa can only openend by one "sink"
- OMX libs are closed source and are not compatible with the "available" I1 SDK.

Audio support is implemented by replacing the original TinyAlsa library with a version that copies the read audio frames to a pipe. This pipe is read by the RTSP server. The RTSP server uses a patched WAVFileSource to read the audio data from the pipe. (Since it tries to read the WAV header 2x I saw no (quick) other way than to hardcode the PCM format into the WAVFileSource code.)

Additionally:
- The OMX ALSA library reads audio in 16-bit 16000Hz stereo, one channel is just empty. To reduce streaming bandwidth the TinyAlsa replacement library converts the stereo data to mono.
- To reduce streaming bandwidth even further, the 16-bit PCM data is converted to 8-bit uLaw and finally results in 128kbit/s.

### RTSP audio support:
The datapath of the audio is as follows:
Mic -> ADC -> Kernel sound driver -> TinyAlsa lib -> OMX ALSA plugin -> Camera application (rmm)

To maintain audio support for the original Yi application, the audio should be cloned at one of the steps with the following in mind:
- Kernel driver can be used by 1 "sink"
- TinyAlsa can only openend by one "sink"
- OMX libs are closed source and are not compatible with the "available" I1 SDK.

Audio support is implemented by replacing the original TinyAlsa library with a version that copies the read audio frames to a pipe. This pipe is read by the RTSP server. The RTSP server uses a patched WAVFileSource to read the audio data from the pipe. (Since it tries to read the WAV header 2x I saw no (quick) other way than to hardcode the PCM format into the WAVFileSource code.)

Additionally:
- The OMX ALSA library reads audio in 16-bit 16000Hz stereo, one channel is just empty. To reduce streaming bandwidth the TinyAlsa replacement library converts the stereo data to mono.
- To reduce streaming bandwidth even further, the 16-bit PCM data is converted to 8-bit uLaw and finally results in 128kbit/s.

## Table of Contents

- [Features](#features)
- [Performance](#performance)
- [Supported cameras](#supported-cameras)
- [Getting started](#getting-started)
- [Build your own firmware](#build-your-own-firmware)
- [Unbricking](#unbricking)
- [Acknowledgments](#acknowledgments)
- [Donation](#donation)
- [Disclaimer](#disclaimer)

## Features
This firmware contains the following features.
Apart from RTSP, snapshot and ONVIF, all the features are copied from the TheCrypt0 project.

- FEATURES
  - RTSP server - allows a RTSP stream of the video (high and/or low resolution) but without audio.
    - rtsp://IP-CAM/ch0_0.h264             (high res)
    - rtsp://IP-CAM:8554/ch0_1.h264        (low res)
  - ONVIF server (with support for h264 stream, snapshot, ptz and presets - standardized interfaces for IP cameras.
  - Snapshot service - allows to get a jpg with a web request.
  Gets the latest yuv image from the kernel buffer and converts it to jpg.
    - http://IP-CAM:8080/cgi-bin/snapshot.sh?res=low        (select resolution: low or high)
    - http://IP-CAM:8080/cgi-bin/snapshot.sh                (default high)
  - MQTT - Motion detection and baby crying detection through mqtt protocol.
  - Web server - web configutation interface (port 8080).
  - SSH server - dropbear.
  - Telnet server - busybox.
  - FTP server.
  - Authentication for HTTP, RTSP and ONVIF server.
  - Proxychains-ng - Disabled by default. Useful if the camera is region locked.
  - Original watermark image removed.
  - The possibility to change some camera settings (copied from official app):
    - camera on/off
    - video saving mode
    - detection sensitivity
    - baby crying detection
    - status led
    - ir led
    - rotate
  - Management of motion detect events and videos through a web page.
  - PTZ support through a web page.
  - The possibility to disable all the cloud features.
  - Online firmware upgrade

## Performance

The performance of the cam is not so good (CPU, RAM, etc...).
If you enable all the services you may have some problems.
For example, enabling both rtsp streams is not recommended.
Disable cloud is recommended to save resources.

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
| **Yi 1080p Home 4FUS** | 4.2.0* | y25 | - |
| **Yi 1080p Home 9FUS** | 4.2.0* | y25 | - |
| **Yi 1080p Home 6FUS** | 2.1.0* | y23 | - |
| **Yi 1080p Home 6FCN** | unknown | y203c | - |

This firmware completely overwrite the original firmware.
So, USE AT YOUR OWN RISK.

**Do not try to use an fw on an unlisted model**

**Do not try to force the fw loading renaming the files**

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
- Or you can use the following docker image https://hub.docker.com/r/borodiliz/yi-hack (thanks to@ borodiliz).
- Copy original home and rootfs partition files to ./stock_firmware/... (don't ask me where to find them).
- git submodule update --init
- ./scripts/init_sysroot.all.sh
- ./scripts/compile.sh
- ./scripts/pack_fw.all.sh

### Dev tips
- If you kill the "rmm" process, the watchdog will reset the camera. This can be prevented by kicking it yourself in a seperate shell: while [ 1 ] ; do sleep 1; echo .; echo V > /dev/watchdog; done
- The "rmm" process can be started with a different set of OMX Libraries by setting environment variables OMX_LIB_PATH and LD_LIBRARY_PATH. That way you dont have to overwrite the original files and possibly brick the system. For instance: OMX_LIB_PATH=/tmp/sd/test/ms LD_LIBRARY_PATH=/home/lib:/lib:/usr/lib:/usr/local/lib:/tmp/sd/test/ms:/home/app/locallib /home/app/rmm
- The I1 SDK is not compatible with this camera. When using the OMX libraries from the system with the headers from the SDK, most applications end up with a "Floating point exception". Possibly the headers do not match the libraries or one of the kernel interfaces.

## Unbricking
If your camera doesn't start repeat the hack as described above (with a lower/higher version) .
You can use the same procedure even if you have a backup copy of the original partitions.
If the bootloader works correctly the camera will restart.

## Acknowledgments
Special thanks to the following people for the previous projects I started from.
- @TheCrypt0 - [https://github.com/TheCrypt0/yi-hack-v4](https://github.com/TheCrypt0/yi-hack-v4)
- @andy2301 - [Ideas for the RTSP](https://github.com/xmflsct/yi-hack-1080p/issues/5#issuecomment-298093437)
- All the people who worked on the previous projects "yi-hack".

## Donation
If you like this project, you can buy me a beer :) 
[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=JBYXDMR24FW7U&currency_code=EUR&source=url)

---
### DISCLAIMER
**I AM NOT RESPONSIBLE FOR ANY USE OR DAMAGE THIS SOFTWARE MAY CAUSE. THIS IS INTENDED FOR EDUCATIONAL PURPOSES ONLY. USE AT YOUR OWN RISK.**
