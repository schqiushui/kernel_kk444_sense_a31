/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 * Copyright (C) 2007 Google Incorporated
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef HTC_UTIL_H
#define HTC_UTIL_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"

struct attribute_status {
       char *title;
       u32 req_value;
       u32 cur_value;
       u32 def_value;
};
void htc_register_camera_bkl(int level);
void htc_register_attrs(struct kobject *led_kobj, struct msm_fb_data_type *mfd);
void htc_set_cabc(struct msm_fb_data_type *mfd);
void htc_reset_status(void);
void htc_dimming_on(struct msm_fb_data_type *mfd);
void htc_dimming_off(void);
void htc_dimming_switch(struct msm_fb_data_type *mfd);
void htc_debugfs_init(struct msm_fb_data_type *mfd);
void htc_set_pp_pa(struct mdss_mdp_ctl *ctl);

#endif 
