<p align="center">
	<img height="200" src="https://user-images.githubusercontent.com/39277388/85948999-59034780-b954-11ea-8fb3-c4c88727cf5b.png">
</p>

<p align="center">
	<a target="_blank" href="https://github.com/roleoroleo/yi-hack-MStar/releases">
		<img src="https://img.shields.io/github/downloads/roleoroleo/yi-hack-MStar/total.svg" alt="Releases Downloads">
	</a>
</p>

# Custom firmware for Yi camera based on MStar platform
This firmware is based on the work done by TheCrypt0
https://github.com/TheCrypt0/yi-hack-v4
It's a clone made for Yi cameras based on MStar platform.

**But you can't choose between TheCrypt0 and MStar: if your cam is based on Hi3518e Chipset then you have to use TheCrypt0, if your cam is based on MStar Infinity Chipset then you have to use MStar.**

I have no time to support the project, so feel free to clone/fork this git and modify it as you want.

## Table of Contents
- [Table of Contents](#table-of-contents)
- [Installation](#installation)
- [Contributing](#contributing-and-bug-reports)
- [Features](#features)
- [RTSP Server](#rtsp-server)
- [Performance](#performance)
- [Supported cameras](#supported-cameras)
- [Is my cam supported?](#is-my-cam-supported)
- [Home Assistant integration](#home-assistant-integration)
- [Build your own firmware](#build-your-own-firmware)
- [Unbricking](#unbricking)
- [License](#license)
- [Acknowledgments](#acknowledgments)
- [Disclaimer](#disclaimer)
- [Donation](#donation)

## Installation

### Install Procedure
1. Check if your cam is supported in the "Supported cameras" section and note the file prefix.
2. Format an SD Card as FAT32. It's recommended to format the card in the camera using the camera's native format function. If the card is already formatted, remove all the files.
3. Download the latest release from [the Releases page](https://github.com/roleoroleo/yi-hack-MStar/releases) based on the file prefix.
4. Extract the contents of the archive to the root of your SD card (for example home_y203c and sys_y203c).
5. Insert the SD Card and reboot the camera.
6. The yellow light will come ON and flash for roughly 30 seconds, which means the firmware is being flashed successfully. The camera will boot up.
7. The yellow light will come ON again for the final stage of flashing. This will take up to 2 minutes.
8. Blue light should come ON indicating that your WiFi connection has been successful (if not disable using app).
9. Go in the browser and access the web interface of the camera as a website (http://IP-CAM) where IP-CAM is the IP address of your cam and you can find it using the mobile app (Camera Settings --> Network Info --> IP Address). If the mobile app can't be paired, you may look for the IP on your router's portal (see connected devices).
10. Done.

### Online Update Procedure
1. Go to the "Maintenance" web page
2. Check if a new release is available
3. Click "Upgrade Firmware"
4. Wait for cam reboot

### Manual Update Procedure
Check the wiki: https://github.com/roleoroleo/yi-hack-MStar/wiki/Manual-firmware-upgrade


### Optional Utilities
Several [optional utilities](https://github.com/roleoroleo/yi-hack-utils) are avaiable, some supporting experimental features like text-to-speech.


## Contributing and Bug Reports
See [CONTRIBUTING](CONTRIBUTING.md)

---

## Features
This custom firmware contains features replicated from the [yi-hack-MStar](https://github.com/roleoroleo/yi-hack-MStar) project and similar to the [yi-hack-v4](https://github.com/TheCrypt0/yi-hack-v4) project.

- FEATURES
  - RTSP server - allows a RTSP stream of the video (high and/or low resolution) and audio (thanks to @PieVo for the work on MStar platform).
    - rtsp://IP-CAM/ch0_0.h264             (high res)
    - rtsp://IP-CAM/ch0_1.h264             (low res)
    - rtsp://IP-CAM/ch0_2.h264             (only audio)
  - ONVIF server (with support for stream, snapshot, ptz, presets, events and WS-Discovery) - standardized interfaces for IP cameras.
  - Snapshot service - allows to get a jpg with a web request.
    - http://IP-CAM/cgi-bin/snapshot.sh?res=low&watermark=yes        (select resolution: low or high, and watermark: yes or no)
    - http://IP-CAM/cgi-bin/snapshot.sh                              (default high without watermark)
  - Timelapse feature
  - MQTT events - Motion detection and baby crying detection through mqtt protocol.
  - MQTT configuration
  - Web server - web configuration interface.
  - SSH server - dropbear.
  - Telnet server - busybox.
  - FTP server.
  - FTP push: export mp4 video to an FTP server (thanks to @Catfriend1).
  - Authentication for HTTP, RTSP and ONVIF server.
  - Proxychains-ng - Disabled by default. Useful if the camera is region locked.
  - Original watermark image removed.
  - The possibility to change some camera settings (copied from official app):
    - camera on/off
    - video saving mode
    - detection sensitivity
    - motion detections (it depends on your cam and your plan)
    - baby crying detection
    - status led
    - ir led
    - rotate
    - ...
  - Management of motion detect events and videos through a web page.
  - View recorded video through a web page (thanks to @BenjaminFaal).
  - PTZ support through a web page (if the cam supports it).
  - PTZ presets.
  - The possibility to disable all the cloud features.
  - Swap File on SD.
  - Online firmware upgrade.
  - Load/save/reset configuration.

- ADDITIONAL FEATURES
  - Google Drive synchronization - https://github.com/roleoroleo/yi-hack-MStar.gdrive
  - Microsoft OneDrive synchronization - https://github.com/denven/yihack-onedrive-uploader  
  - rsync synchronization thanks to @hetzbh - https://github.com/hetzbh/yi-cams-backup

## RTSP Server
I wrote a daemon that reads the video stream directly from the kernel driver memory and sends it to an application based on live555.
I was inspired by the following topic:
- @andy2301 - [Ideas for the RSTP rtsp and rtsp2301](https://github.com/xmflsct/yi-hack-1080p/issues/5#issuecomment-294326131)

The RTSP server code derives from live555 - http://www.live555.com/ and from the archive rtsp2303_srcbin_20170414-1630.zip posted in the link above.

### RTSP audio support (many thanks to @PieVo for adding it)
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
| **Yi 1080p Dome 4FCN** | 4.6.0* | h201c | - |
| **Yi 1080p Home 4FUS** | 4.2.0* | y25 | - |
| **Yi 1080p Home 9FUS** | 4.2.0* | y25 | - |
| **XiaoYi Camera Y3 9FCN**| 4.2.0* | y25 | - |
| **Yi 1080p Home 6FUS** | 2.1.0* | y23 | - |
| **Yi 1080p Home 6FCN** | unknown | y203c | - |
| **Yi 1080p Home 4FCN** | unknown | y23 | - |
| **YI Dome Camera X** | 4.0.0* | y30 | Experimental |
| **YI Home Camera H7** | 4.4.0* | h307 | Experimental |
| **ieGeek IE80** | 8.0.0* | h305r | Experimental |


This firmware completely overwrite the original firmware.
So, USE AT YOUR OWN RISK.

**Do not try to use a fw on an unlisted model**

**Do not try to force the fw loading renaming the files**

## Is my cam supported?
If you want to know if your cam is supported, please check the serial number (first 4 letters) and the firmware version.
If both numbers appear in the same row in the table above, your cam is supported.
If not, check the other projects related to Yi cams:
- https://github.com/TheCrypt0/yi-hack-v4 and previous
- https://github.com/roleoroleo/yi-hack-Allwinner
- https://github.com/roleoroleo/yi-hack-Allwinner-v2

## Home Assistant integration
Are you using Home Assistant? Do you want to integrate your cam? Try these custom integrations:
- https://github.com/roleoroleo/yi-hack_ha_integration
- https://github.com/AlexxIT/WebRTC

You can also use the [web services](https://github.com/roleoroleo/yi-hack-MStar/wiki/Web-services-description) in Home Assistant -- here's one way to do that. (This example requires the nanotts optional utility to be installed on the camera.) Set up a rest_command in your configuration.yaml to call one of the [web services](https://github.com/roleoroleo/yi-hack-MStar/wiki/Web-services-description). 
```
rest_command:
  camera_announce:
    url: http://[camera address]/cgi-bin/speak.sh?lang={{language}}&voldb={{volume}}
    method: POST
    payload: "{{message}}"
```
Create an automation and use yaml in the action to send data to the web service. 
```
service: rest_command.camera_announce
data:
  language: en-US
  message: "All your base are belong to us."
  volume: '-8'
``` 

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

----

## License
[GPLv3](https://choosealicense.com/licenses/gpl-3.0/)

## Acknowledgments
Special thanks to the following people for the previous projects I started from.
- @TheCrypt0 - [https://github.com/TheCrypt0/yi-hack-v4](https://github.com/TheCrypt0/yi-hack-v4)
- @andy2301 - [Ideas for the RTSP](https://github.com/xmflsct/yi-hack-1080p/issues/5#issuecomment-298093437)
- All the people who worked on the previous projects "yi-hack".

## DISCLAIMER
**NOBODY BUT YOU IS RESPONSIBLE FOR ANY USE OR DAMAGE THIS SOFTWARE MAY CAUSE. THIS IS INTENDED FOR EDUCATIONAL PURPOSES ONLY. USE AT YOUR OWN RISK.**

## Donation
If you like this project, you can buy roleo a beer :)

Click [here](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=JBYXDMR24FW7U&currency_code=EUR&source=url) or use the below QR code to donate via PayPal
<p align="center">
  <img src="https://github.com/roleoroleo/yi-hack-MStar/assets/39277388/35b9db58-4013-4bf9-845c-2028a769662d"/>
</p>
