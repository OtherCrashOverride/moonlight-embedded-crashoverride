# Moonlight Embedded

Moonlight Embedded is an open source implementation of NVIDIA's GameStream, as used by the NVIDIA Shield, but built for Linux.

This is a fork for Odroid C1/C2.

## Dependencies
`sudo apt install cmake libasound2-dev libopus-dev libevdev-dev libudev-dev libcurl4-openssl-dev libssl-dev libavahi-client-dev libenet-dev`

## Build
```
git clone https://github.com/OtherCrashOverride/moonlight-embedded-crashoverride.git
cd moonlight-embedded-crashoverride
git submodule update --init
mkdir build
cd build/
cmake -D CMAKE_BUILD_TYPE=Release ../
make
make install
```

## Discussion
[ODROID Forum](http://forum.odroid.com/viewtopic.php?f=91&t=15456) Moonlight Embedded on ODROID  


