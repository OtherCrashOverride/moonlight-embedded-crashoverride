# Moonlight Embedded

Moonlight Embedded is an open source implementation of NVIDIA's GameStream, as used by the NVIDIA Shield, but built for Linux.

This is a fork for Odroid XU3/XU4.

## Dependencies
`sudo apt install cmake libasound2-dev libopus-dev libevdev-dev libudev-dev libcurl4-openssl-dev libssl-dev libavahi-client-dev libenet-dev premake4`

## Build
```
git clone https://github.com/OtherCrashOverride/moonlight-embedded-crashoverride.git -b xu4
cd moonlight-embedded-crashoverride
git submodule update --init
premake4 gmake
make
```

## Discussion
[ODROID Forum](http://forum.odroid.com/viewtopic.php?f=91&t=15456) Moonlight Embedded on ODROID  

