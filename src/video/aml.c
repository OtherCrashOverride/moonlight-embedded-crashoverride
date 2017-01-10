/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015, 2016 Iwan Timmer
 * Copyright (C) 2016 OtherCrashOverride, Daniel Mehrwald
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include <Limelight.h>

#include <sys/utsname.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <codec.h>
#include <sys/ioctl.h>

#define SYNC_OUTSIDE 0x02
#define UCODE_IP_ONLY_PARAM 0x08

 /* video freerun mode */
#define FREERUN_NONE    0	/* no freerun mode */
#define FREERUN_NODUR   1	/* freerun without duration */
#define FREERUN_DUR		2	/* freerun with duration */


#define FBIOBLANK               0x4611          /* arg: 0 or vesa level + 1 */

 /* VESA Blanking Levels */
#define VESA_NO_BLANKING        0
#define VESA_VSYNC_SUSPEND      1
#define VESA_HSYNC_SUSPEND      2
#define VESA_POWERDOWN          3

enum {
	/* screen: unblanked, hsync: on,  vsync: on */
	FB_BLANK_UNBLANK = VESA_NO_BLANKING,

	/* screen: blanked,   hsync: on,  vsync: on */
	FB_BLANK_NORMAL = VESA_NO_BLANKING + 1,

	/* screen: blanked,   hsync: on,  vsync: off */
	FB_BLANK_VSYNC_SUSPEND = VESA_VSYNC_SUSPEND + 1,

	/* screen: blanked,   hsync: off, vsync: on */
	FB_BLANK_HSYNC_SUSPEND = VESA_HSYNC_SUSPEND + 1,

	/* screen: blanked,   hsync: off, vsync: off */
	FB_BLANK_POWERDOWN = VESA_POWERDOWN + 1
};


static codec_para_t codecParam = { 0 };


static void set_osd_blank_enabled(int osd, bool value)
{
	if (osd < 0 || osd > 1)
	{
		printf("set_osd_blank_enabled: invalid osd (%d)\n", osd);
		return;
	}

	const char* path;
	switch (osd)
	{
	case 0:
		path = "/dev/fb0";
		break;

	case 1:
		path = "/dev/fb1";
		break;

	default:
		printf("set_osd_blank_enabled: invalid osd (%d)\n", osd);
		return;
	}


	int fd = open(path, O_RDWR | O_TRUNC);
	if (fd < 0)
	{
		printf("could not open OSD (%x)\n", fd);
	}
	else
	{
		int ret = ioctl(fd, FBIOBLANK, value ? FB_BLANK_NORMAL : FB_BLANK_UNBLANK);
		if (ret < 0)
		{
			printf("FBIOBLANK failed (%x)\n", ret);
		}

		close(fd);
	}
}

static void set_freerun_enabled(bool value)
{
	int fd = open("/dev/amvideo", O_RDWR | O_TRUNC);
	if (fd < 0)
	{
		printf("could not open /dev/amvideo (%x)\n", fd);
	}
	else
	{
		int ret = ioctl(fd, AMSTREAM_IOC_SET_FREERUN_MODE, value ? FREERUN_NODUR : FREERUN_NONE);
		if (ret < 0)
		{
			printf("AMSTREAM_IOC_SET_FREERUN_MODE failed (%x)\n", ret);
		}

		close(fd);
	}
}

void aml_setup(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
  set_osd_blank_enabled(0, true);
  set_osd_blank_enabled(1, true);

  set_freerun_enabled(true);

  codecParam.stream_type = STREAM_TYPE_ES_VIDEO;
  codecParam.has_video = 1;
  codecParam.noblock = 0;
  codecParam.am_sysinfo.param = 0;

  switch (videoFormat) {
    case VIDEO_FORMAT_H264:
      if (width > 1920 || height > 1080) {
        codecParam.video_type = VFORMAT_H264_4K2K;
        codecParam.am_sysinfo.format = VIDEO_DEC_FORMAT_H264_4K2K;
      } else {
        codecParam.video_type = VFORMAT_H264;
        codecParam.am_sysinfo.format = VIDEO_DEC_FORMAT_H264;

        // Workaround for decoding special case of C1, 1080p, H264
        int major, minor;
        struct utsname name;
        uname(&name);
        int ret = sscanf(name.release, "%d.%d", &major, &minor);
        if (!(major > 3 || (major == 3 && minor >= 14)) && width == 1920 && height == 1080)
            codecParam.am_sysinfo.param = (void*) UCODE_IP_ONLY_PARAM;
      }
      break;
    case VIDEO_FORMAT_H265:
      codecParam.video_type = VFORMAT_HEVC;
      codecParam.am_sysinfo.format = VIDEO_DEC_FORMAT_HEVC;
      break;
    default:
      printf("Video format not supported\n");
      exit(1);
  }

  codecParam.am_sysinfo.width = width;
  codecParam.am_sysinfo.height = height;
  codecParam.am_sysinfo.rate = 96000 / redrawRate;
  codecParam.am_sysinfo.param = (void*) ((size_t) codecParam.am_sysinfo.param | SYNC_OUTSIDE);

  int api = codec_init(&codecParam);
  if (api != 0) {
    fprintf(stderr, "codec_init error: %x\n", api);
    exit(1);
  }
}

void aml_cleanup() {
  int api = codec_close(&codecParam);

  set_osd_blank_enabled(0, false);
  set_osd_blank_enabled(1, false);

  set_freerun_enabled(false);
}

int aml_submit_decode_unit(PDECODE_UNIT decodeUnit) {
  int result = DR_OK;
  PLENTRY entry = decodeUnit->bufferList;
  while (entry != NULL) {
    int api = codec_write(&codecParam, entry->data, entry->length);
    if (api != entry->length) {
      fprintf(stderr, "codec_write error: %x\n", api);
      codec_reset(&codecParam);
      result = DR_NEED_IDR;
      break;
    }

    entry = entry->next;
  }
  return result;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_aml = {
  .setup = aml_setup,
  .cleanup = aml_cleanup,
  .submitDecodeUnit = aml_submit_decode_unit,
  .capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_SLICES_PER_FRAME(8),
};
