#!lua
local output = "./build/" .. _ACTION

solution "moonlight-embedded"
   configurations { "Debug", "Release" }

project "moonlight-common-c"
   location (output)
   kind "StaticLib"
   language "C"
   includedirs { "third_party/moonlight-common-c/reedsolomon" }
   files { "third_party/moonlight-common-c/**.h", "third_party/moonlight-common-c/**.c" }
   buildoptions { "-fPIC" }
   -- linkoptions { "-lenet -lpthread -lssl -lcrypto" }
   links {"pthread", "enet", "ssl", "crypto"}

   configuration "Debug"
      flags { "Symbols" }
      defines { "DEBUG" }

   configuration "Release"
      flags { "Optimize" }
      defines { "NDEBUG" }

project "h264bitstream"
   location (output)
   kind "StaticLib"
   language "C"
   includedirs { "" }
   files { "third_party/h264bitstream/**.h", "third_party/h264bitstream/**.c" }
   buildoptions { "-fPIC" }
   -- linkoptions { "" }

   configuration "Debug"
      flags { "Symbols" }
      defines { "DEBUG" }

   configuration "Release"
      flags { "Optimize" }
      defines { "NDEBUG" }

project "gamestream"
   location (output)
   kind "StaticLib"
   language "C"
   includedirs { "third_party/moonlight-common-c/src", "third_party/h264bitstream/" }
   files { "libgamestream/**.h", "libgamestream/**.c" }
   buildoptions { "-fPIC" }
   linkoptions { "" }
   -- links {"moonlight-common-c", "h264bitstream"}
   links {"uuid", "expat", "avahi-client", "curl", "h264bitstream", "moonlight-common-c"}

   configuration "Debug"
      flags { "Symbols" }
      defines { "DEBUG" }

   configuration "Release"
      flags { "Optimize" }
      defines { "NDEBUG" }

project "xu4"
	targetname ("moonlight-embedded")
   location (output)
   kind "ConsoleApp"
   language "C"
   includedirs { "libgamestream", "third_party/moonlight-common-c/src", "/usr/include/libevdev-1.0", "/usr/include/opus",
				 "/usr/include/libdrm" }
   files { "src/*.h", "src/*.c", "xu4/fake_video.cpp", "src/audio/fake.c", "src/input/evdev.c", "src/input/udev.c", "src/input/mapping.c",
		   "xu4/*.h", "xu4/*.cpp" }
   buildoptions { "--std=c++11" }
   links {"pthread", "udev", "evdev", "asound", "opus", "moonlight-common-c", "h264bitstream", "gamestream",
		  "enet", "ssl", "crypto",
		  "uuid", "expat", "avahi-client", "avahi-common", "curl",
		  "m", "X11", "mali", "stdc++", "drm", "dri2", "openal"}
   defines { "HAVE_FAKE=1" }

   configuration "Debug"
      flags { "Symbols" }
      defines { "DEBUG" }

   configuration "Release"
      flags { "Optimize" }
      defines { "NDEBUG" }