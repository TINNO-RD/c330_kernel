/*
 *
 * Tinno project info driver.
 * 
 * Copyright (c) 2010-2015, Tinno Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __TINNO_PROJECT_INFO_H__
#define __TINNO_PROJECT_INFO_H__

#include <linux/device.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/err.h>

#define TINNO_PROJECT_PROPTITY_NAME 	"qcom,tinno-hardware"

char * tinno_get_project_name(void);
char * tinno_get_market_area(void);   
char * tinno_get_sensor_info(void);       //if not MBA ,default is NULL, should verify NULL
bool tinno_project_is(char *project_name);

#endif
