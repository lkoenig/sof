/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __PLATFORM_IDC_H__
#define __PLATFORM_IDC_H__

struct idc_msg;

static inline int idc_send_msg(struct idc_msg *msg,
			       uint32_t mode) { return 0; }

static inline int idc_init(void) { return 0; }

#endif /* __PLATFORM_IDC_H__ */
