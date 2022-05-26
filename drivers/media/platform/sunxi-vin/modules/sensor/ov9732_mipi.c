/*
 * A V4L2 driver for ov9732_mipi cameras.
 *
 * Copyright (c) 2018 by junhuanchen Co., Ltd.
 *
 * Authors:  junhuanchen <junhuanchen@qq.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>

#include "camera.h"
#include "sensor_helper.h"

MODULE_AUTHOR("dalaoshu");
MODULE_DESCRIPTION("A low-level driver for ov9732 sensors");
MODULE_LICENSE("GPL");

#define MCLK (24 * 1000 * 1000)
#define V4L2_IDENT_SENSOR 0x9732

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 30

/*
 * The ov9732_mipi sits on i2c with ID 0xC0
 */
#define I2C_ADDR 0x6c

#define SENSOR_NAME "ov9732_mipi"

#define USE_30FPS 1

struct cfg_array { /* coming later */
  struct regval_list *regs;
  int size;
};

/*
 * The default register settings
 *
 */
static struct regval_list sensor_default_regs[] = {

};

/* 640x480 Raw10 30fps 24MHz */
static struct regval_list sensor_VGA_regs[] = {
    {0x0103, 0x01}, {0x0100, 0x00}, {0x3001, 0x00}, {0x3002, 0x00},
    {0x3007, 0x1f}, {0x3008, 0xff}, {0x3009, 0x02}, {0x3010, 0x00},
    {0x3011, 0x08}, {0x3014, 0x22}, {0x301e, 0x15}, {0x3030, 0x19},
    {0x3080, 0x02}, {0x3081, 0x3c}, {0x3082, 0x04}, {0x3083, 0x00},
    {0x3084, 0x02}, {0x3085, 0x01}, {0x3086, 0x01}, {0x3089, 0x01},
    {0x308a, 0x00}, {0x3103, 0x01}, {0x3600, 0xf6}, {0x3601, 0x72},
    {0x3605, 0x66}, {0x3610, 0x0c}, {0x3611, 0x60}, {0x3612, 0x35},
    {0x3654, 0x10}, {0x3655, 0x77}, {0x3656, 0x77}, {0x3657, 0x07},
    {0x3658, 0x22}, {0x3659, 0x22}, {0x365a, 0x02}, {0x3700, 0x1f},
    {0x3701, 0x10}, {0x3702, 0x0c}, {0x3703, 0x0b}, {0x3704, 0x3c},
    {0x3705, 0x51}, {0x370d, 0x20}, {0x3710, 0x0d}, {0x3782, 0x58},
    {0x3783, 0x60}, {0x3784, 0x05}, {0x3785, 0x55}, {0x37c0, 0x07},
    {0x3800, 0x00}, {0x3801, 0x04}, {0x3802, 0x00}, {0x3803, 0x04},
    {0x3804, 0x05}, {0x3805, 0x0b}, {0x3806, 0x02}, {0x3807, 0xdb},
    {0x3808, 0x05}, {0x3809, 0x00}, {0x380a, 0x02}, {0x380b, 0xd0},
    {0x380c, 0x05}, {0x380d, 0xc6}, {0x380e, 0x03}, {0x380f, 0x22},
    {0x3810, 0x00}, {0x3811, 0x04}, {0x3812, 0x00}, {0x3813, 0x04},
    {0x3816, 0x00}, {0x3817, 0x00}, {0x3818, 0x00}, {0x3819, 0x04},
    {0x3820, 0x10}, {0x3821, 0x00}, {0x382c, 0x06}, {0x3500, 0x00},
    {0x3501, 0x31}, {0x3502, 0x00}, {0x3503, 0x03}, {0x3504, 0x00},
    {0x3505, 0x00}, {0x3509, 0x10}, {0x350a, 0x00}, {0x350b, 0x40},
    {0x3d00, 0x00}, {0x3d01, 0x00}, {0x3d02, 0x00}, {0x3d03, 0x00},
    {0x3d04, 0x00}, {0x3d05, 0x00}, {0x3d06, 0x00}, {0x3d07, 0x00},
    {0x3d08, 0x00}, {0x3d09, 0x00}, {0x3d0a, 0x00}, {0x3d0b, 0x00},
    {0x3d0c, 0x00}, {0x3d0d, 0x00}, {0x3d0e, 0x00}, {0x3d0f, 0x00},
    {0x3d80, 0x00}, {0x3d81, 0x00}, {0x3d82, 0x38}, {0x3d83, 0xa4},
    {0x3d84, 0x00}, {0x3d85, 0x00}, {0x3d86, 0x1f}, {0x3d87, 0x03},
    {0x3d8b, 0x00}, {0x3d8f, 0x00}, {0x4001, 0xe0}, {0x4004, 0x00},
    {0x4005, 0x02}, {0x4006, 0x01}, {0x4007, 0x40}, {0x4009, 0x0b},
    {0x4300, 0x03}, {0x4301, 0xff}, {0x4304, 0x00}, {0x4305, 0x00},
    {0x4309, 0x00}, {0x4600, 0x00}, {0x4601, 0x04}, {0x4800, 0x00},
    {0x4805, 0x00}, {0x4821, 0x50}, {0x4823, 0x50}, {0x4837, 0x2d},
    {0x4a00, 0x00}, {0x4f00, 0x80}, {0x4f01, 0x10}, {0x4f02, 0x00},
    {0x4f03, 0x00}, {0x4f04, 0x00}, {0x4f05, 0x00}, {0x4f06, 0x00},
    {0x4f07, 0x00}, {0x4f08, 0x00}, {0x4f09, 0x00}, {0x5000, 0x07},
    {0x500c, 0x00}, {0x500d, 0x00}, {0x500e, 0x00}, {0x500f, 0x00},
    {0x5010, 0x00}, {0x5011, 0x00}, {0x5012, 0x00}, {0x5013, 0x00},
    {0x5014, 0x00}, {0x5015, 0x00}, {0x5016, 0x00}, {0x5017, 0x00},
    {0x5080, 0x00}, {0x5180, 0x01}, {0x5181, 0x00}, {0x5182, 0x01},
    {0x5183, 0x00}, {0x5184, 0x01}, {0x5185, 0x00}, {0x5708, 0x06},
    {0x5781, 0x0e}, {0x5783, 0x0f}, {0x3603, 0x70}, {0x3620, 0x1e},
    {0x400a, 0x01}, {0x400b, 0xc0}, {0x0100, 0x01},
};

/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */
static struct regval_list sensor_fmt_raw[] = {

};

/*
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function ,retrun -EINVAL
 */

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value) {
  struct sensor_info *info = to_state(sd);

  *value = info->exp;
  sensor_dbg("sensor_get_exposure = %d\n", info->exp);

  return 0;
}

static int ov9732_sensor_vts;
static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val) {
  data_type explow, expmid, exphigh;
  struct sensor_info *info = to_state(sd);

  if (exp_val > ((ov9732_sensor_vts - 16) << 4))
    exp_val = (ov9732_sensor_vts - 16) << 4;
  if (exp_val < 16)
    exp_val = 16;

  exphigh = (unsigned char)((exp_val >> 16) & 0x0F);
  expmid = (unsigned char)((exp_val >> 8) & 0xFF);
  explow = (unsigned char)(exp_val & 0xFF);

  sensor_write(sd, 0x3500, exphigh);
  sensor_write(sd, 0x3501, expmid);
  sensor_write(sd, 0x3502, explow);

  sensor_dbg("sensor_s_exp info->exp %d\n", exp_val);
  info->exp = exp_val;

  return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value) {
  struct sensor_info *info = to_state(sd);

  *value = info->gain;
  sensor_dbg("sensor_get_gain = %d\n", info->gain);

  return 0;
}

static int last_dgain;
static int sensor_s_gain(struct v4l2_subdev *sd, unsigned int gain_val) {
  data_type gainlow;
  struct sensor_info *info = to_state(sd);
  unsigned int gain_dig;

  if (gain_val < 1 * 16)
    gain_val = 16;
  if (gain_val > 64 * 16 - 1)
    gain_val = 64 * 16 - 1;

  if (gain_val < 32) {
    gainlow = 0x10;
    gain_dig = gain_val << 6;
  } else if (gain_val < 64) {
    gainlow = 0x20;
    gain_dig = gain_val << 5;
  } else if (gain_val < 128) {
    gainlow = 0x42;
    gain_dig = gain_val << 4;
  } else {
    gainlow = 0x8A;
    gain_dig = gain_val << 3;
  }

  /* sensor_write(sd, 0x350a, gainhigh); */
  sensor_write(sd, 0x350b, gainlow);

  sensor_write(sd, 0x3400, last_dgain >> 8);
  sensor_write(sd, 0x3401, last_dgain & 0xff);

  sensor_write(sd, 0x3402, last_dgain >> 8);
  sensor_write(sd, 0x3403, last_dgain & 0xff);

  sensor_write(sd, 0x3404, last_dgain >> 8);
  sensor_write(sd, 0x3405, last_dgain & 0xff);

  last_dgain = gain_dig;

  sensor_dbg("sensor_s_gain info->gain %d %d\n", gain_val, last_dgain);
  info->gain = gain_val;

  return 0;
}

static int frame_cnt;
static int last_exp;
static int last_gain;
static unsigned int exp_duration;
static int sensor_s_exp_gain(struct v4l2_subdev *sd,
                             struct sensor_exp_gain *exp_gain) {
  struct sensor_info *info = to_state(sd);
  data_type duration_mid, duration_low;

  sensor_write(sd, 0x3208, 0x00);
  sensor_s_exp(sd, last_exp);
  sensor_s_gain(sd, last_gain);
  sensor_write(sd, 0x3208, 0x10);
  sensor_write(sd, 0x3208, 0xa0);

  last_exp = exp_gain->exp_val;
  last_gain = exp_gain->gain_val;

  /* STROBE 20us */
  exp_duration = (info->exp >> 4) * 960 / 48 / 20;

  /* sync MCU */
  if (frame_cnt < 5) {
    sensor_print("%s frame_cnt %d\n", __func__, frame_cnt);
    frame_cnt++;
    exp_duration = 2500;
  }

  duration_mid = (unsigned char)((exp_duration >> 8) & 0xFF);
  duration_low = (unsigned char)(exp_duration & 0xFF);
  sensor_write(sd, 0x3b8e, duration_mid);
  sensor_write(sd, 0x3b8f, duration_low);

  /* vsync 20ns */
  /* exp_duration = exp_duration * 12; */

  duration_mid = (unsigned char)((exp_duration >> 8) & 0xFF);
  duration_low = (unsigned char)(exp_duration & 0xFF);
  sensor_write(sd, 0x4311, duration_mid);
  sensor_write(sd, 0x4312, duration_low);

  return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off) {
  int ret;
  data_type rdval;

  ret = sensor_read(sd, 0x0100, &rdval);

  if (ret != 0)
    return ret;

  if (on_off == STBY_ON)
    ret = sensor_write(sd, 0x0100, rdval & 0xfe);
  else
    ret = sensor_write(sd, 0x0100, rdval | 0x01);

  return ret;
}

/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on) {
  int ret;

  ret = 0;
  switch (on) {
  case STBY_ON:
    ret = sensor_s_sw_stby(sd, STBY_ON);
    if (ret < 0)
      sensor_err("soft stby falied!\n");
    usleep_range(10000, 12000);

    cci_lock(sd);
    /* inactive mclk after stadby in */
    vin_set_mclk(sd, OFF);
    cci_unlock(sd);
    break;
  case STBY_OFF:
    cci_lock(sd);

    vin_set_mclk_freq(sd, MCLK);
    vin_set_mclk(sd, ON);
    usleep_range(10000, 12000);

    cci_unlock(sd);
    ret = sensor_s_sw_stby(sd, STBY_OFF);
    if (ret < 0)
      sensor_err("soft stby off falied!\n");
    usleep_range(10000, 12000);

    break;
  case PWR_ON:
    sensor_print("PWR_ON!\n");

    cci_lock(sd);

    vin_gpio_set_status(sd, RESET, 1);

    vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
    usleep_range(1000, 1200);
    usleep_range(30000, 31000);
    vin_set_mclk_freq(sd, MCLK);
    vin_set_mclk(sd, ON);
    usleep_range(10000, 12000);

    vin_set_pmu_channel(sd, AVDD, ON);
    usleep_range(30000, 31000);

    vin_set_pmu_channel(sd, IOVDD, ON);
    usleep_range(30000, 31000);

    vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
    usleep_range(30000, 31000);
    cci_unlock(sd);
    break;
  case PWR_OFF:
    sensor_print("PWR_OFF!\n");
    cci_lock(sd);

    vin_set_mclk(sd, OFF);
    vin_gpio_write(sd, RESET, CSI_GPIO_LOW);

    vin_set_pmu_channel(sd, AVDD, OFF);
    vin_set_pmu_channel(sd, IOVDD, OFF);

    vin_gpio_set_status(sd, RESET, 0);

    cci_unlock(sd);
    break;
  default:
    return -EINVAL;
  }

  return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val) {
  sensor_print("%s val %d\n", __func__, val);

  switch (val) {
  case 0:
    vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
    usleep_range(10000, 12000);
    break;
  case 1:
    vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
    usleep_range(10000, 12000);
    break;
  default:
    return -EINVAL;
  }
  return 0;
}

static int sensor_detect(struct v4l2_subdev *sd) {
  data_type rdval = 0;

  sensor_read(sd, 0x300A, &rdval);
  if (rdval != (V4L2_IDENT_SENSOR >> 8))
    return -ENODEV;

  sensor_read(sd, 0x300B, &rdval);
  if (rdval != (V4L2_IDENT_SENSOR & 0xff))
    return -ENODEV;

  return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val) {
  int ret;
  struct sensor_info *info = to_state(sd);

  sensor_dbg("sensor_init\n");

  /*Make sure it is a target sensor */
  ret = sensor_detect(sd);
  if (ret) {
    sensor_err("chip found is not an target chip.\n");
    return ret;
  }

  info->focus_status = 0;
  info->low_speed = 0;
  info->width = HD720_WIDTH;
  info->height = HD720_HEIGHT;
  info->hflip = 0;
  info->vflip = 0;
  info->exp = 0;
  info->gain = 0;

  info->tpf.numerator = 1;
  info->tpf.denominator = SENSOR_FRAME_RATE; /* 30 fps */
  info->preview_first_flag = 1;

  return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg) {
  int ret = 0;
  struct sensor_info *info = to_state(sd);

  switch (cmd) {
  case GET_CURRENT_WIN_CFG:
    if (info->current_wins != NULL) {
      memcpy(arg, info->current_wins, sizeof(struct sensor_win_size));
      ret = 0;
    } else {
      sensor_err("empty wins!\n");
      ret = -1;
    }
    break;
  case SET_FPS:
    ret = 0;
    break;
  case VIDIOC_VIN_SENSOR_EXP_GAIN:
    ret = sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
    break;
  case VIDIOC_VIN_SENSOR_CFG_REQ:
    sensor_cfg_req(sd, (struct sensor_config *)arg);
    break;
  default:
    return -EINVAL;
  }
  return ret;
}

/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
    {.desc = "Raw RGB Bayer",
     .mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
     .regs = sensor_fmt_raw,
     .regs_size = ARRAY_SIZE(sensor_fmt_raw),
     .bpp = 1},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */
static struct sensor_win_size sensor_win_sizes[] = {
#if USE_30FPS
    {
        .width = HD720_WIDTH,
        .height = HD720_HEIGHT,
        .hoffset = 0,
        .voffset = 0,
        .hts = 0x0322,
        .vts = 0x05c6,
        .pclk = 48 * 1000 * 1000,
        .mipi_bps = 93 * 1000 * 1000,
        .fps_fixed = 30,
        .bin_factor = 1,
        .intg_min = 1 << 4,
        .intg_max = (0x05c6) << 4,
        .gain_min = 1 << 4,
        .gain_max = 16 << 4,
        .regs = sensor_VGA_regs,
        .regs_size = ARRAY_SIZE(sensor_VGA_regs),
        .set_size = NULL,
    },
#endif
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
                                struct v4l2_mbus_config *cfg) {
  cfg->type = V4L2_MBUS_CSI2;
  cfg->flags = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0;

  return 0;
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl) {
  struct sensor_info *info =
      container_of(ctrl->handler, struct sensor_info, handler);
  struct v4l2_subdev *sd = &info->sd;

  switch (ctrl->id) {
  case V4L2_CID_GAIN:
    return sensor_g_gain(sd, &ctrl->val);
  case V4L2_CID_EXPOSURE:
    return sensor_g_exp(sd, &ctrl->val);
  }
  return -EINVAL;
}

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl) {
  struct sensor_info *info =
      container_of(ctrl->handler, struct sensor_info, handler);
  struct v4l2_subdev *sd = &info->sd;

  switch (ctrl->id) {
  case V4L2_CID_GAIN:
    return sensor_s_gain(sd, ctrl->val);
  case V4L2_CID_EXPOSURE:
    return sensor_s_exp(sd, ctrl->val);
  }
  return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info) {
  int ret;
  struct v4l2_subdev *sd = &info->sd;
  struct sensor_format_struct *sensor_fmt = info->fmt;
  struct sensor_win_size *wsize = info->current_wins;

  ret = sensor_write_array(sd, sensor_default_regs,
                           ARRAY_SIZE(sensor_default_regs));
  if (ret < 0) {
    sensor_err("write sensor_default_regs error\n");
    return ret;
  }

  sensor_dbg("sensor_reg_init\n");

  sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

  if (wsize->regs)
    sensor_write_array(sd, wsize->regs, wsize->regs_size);

  if (wsize->set_size)
    wsize->set_size(sd);

  info->width = wsize->width;
  info->height = wsize->height;
  ov9732_sensor_vts = wsize->vts;
  info->exp = 0;
  info->gain = 0;

  frame_cnt = 0;

  sensor_print("s_fmt set width = %d, height = %d\n", wsize->width,
               wsize->height);

  return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable) {
  struct sensor_info *info = to_state(sd);

  sensor_print("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
               info->current_wins->width, info->current_wins->height,
               info->current_wins->fps_fixed, info->fmt->mbus_code);

  if (!enable) {
    /* stream off */
    sensor_write(sd, 0x0100, 0x00);
    return 0;
  }

  return sensor_reg_init(info);
}

/* ----------------------------------------------------------------------- */

static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
    .g_volatile_ctrl = sensor_g_ctrl,
    .s_ctrl = sensor_s_ctrl,
};

static const struct v4l2_subdev_core_ops sensor_core_ops = {
    .reset = sensor_reset,
    .init = sensor_init,
    .s_power = sensor_power,
    .ioctl = sensor_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl32 = sensor_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
    .s_parm = sensor_s_parm,
    .g_parm = sensor_g_parm,
    .s_stream = sensor_s_stream,
    .g_mbus_config = sensor_g_mbus_config,
};

static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
    .enum_mbus_code = sensor_enum_mbus_code,
    .enum_frame_size = sensor_enum_frame_size,
    .get_fmt = sensor_get_fmt,
    .set_fmt = sensor_set_fmt,
};

static const struct v4l2_subdev_ops sensor_ops = {
    .core = &sensor_core_ops,
    .video = &sensor_video_ops,
    .pad = &sensor_pad_ops,
};

/* ----------------------------------------------------------------------- */
static struct cci_driver cci_drv = {
    .name = SENSOR_NAME,
    .addr_width = CCI_BITS_16,
    .data_width = CCI_BITS_8,
};

static const struct v4l2_ctrl_config sensor_custom_ctrls[] = {
    {
        .ops = &sensor_ctrl_ops,
        .id = V4L2_CID_FRAME_RATE,
        .name = "frame rate",
        .type = V4L2_CTRL_TYPE_INTEGER,
        .min = 15,
        .max = 120,
        .step = 1,
        .def = 120,
    },
};

static int sensor_init_controls(struct v4l2_subdev *sd,
                                const struct v4l2_ctrl_ops *ops) {
  struct sensor_info *info = to_state(sd);
  struct v4l2_ctrl_handler *handler = &info->handler;
  struct v4l2_ctrl *ctrl;
  int ret = 0;

  v4l2_ctrl_handler_init(handler, 2);

  v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 16, 256 * 16, 1, 16);
  ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 3 * 16, 65536 * 16,
                           1, 3 * 16);
  if (ctrl != NULL)
    ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

  if (handler->error) {
    ret = handler->error;
    v4l2_ctrl_handler_free(handler);
  }

  sd->ctrl_handler = handler;

  return ret;
}

static int sensor_probe(struct i2c_client *client,
                        const struct i2c_device_id *id) {
  struct v4l2_subdev *sd;
  struct sensor_info *info;

  info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
  if (info == NULL)
    return -ENOMEM;
  sd = &info->sd;

  cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv);
  sensor_init_controls(sd, &sensor_ctrl_ops);

  mutex_init(&info->lock);

  info->fmt = &sensor_formats[0];
  info->fmt_pt = &sensor_formats[0];
  info->win_pt = &sensor_win_sizes[0];
  info->fmt_num = N_FMTS;
  info->win_size_num = N_WIN_SIZES;
  info->sensor_field = V4L2_FIELD_NONE;
  info->stream_seq = MIPI_BEFORE_SENSOR;
  info->af_first_flag = 1;
  info->exp = 0;
  info->gain = 0;

  return 0;
}

static int sensor_remove(struct i2c_client *client) {
  struct v4l2_subdev *sd;

  sd = cci_dev_remove_helper(client, &cci_drv);

  kfree(to_state(sd));
  return 0;
}

static const struct i2c_device_id sensor_id[] = {{SENSOR_NAME, 0}, {}};

MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
    .driver =
        {
            .owner = THIS_MODULE,
            .name = SENSOR_NAME,
        },
    .probe = sensor_probe,
    .remove = sensor_remove,
    .id_table = sensor_id,
};
static __init int init_sensor(void) {
  return cci_dev_init_helper(&sensor_driver);
}

static __exit void exit_sensor(void) { cci_dev_exit_helper(&sensor_driver); }

module_init(init_sensor);
module_exit(exit_sensor);
