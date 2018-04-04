/*
 * mtk_audio_device.h
 *
 *  Created on: 2013/10/23
 *      Author: MTK04880
 */

#ifndef MTK_AUDIO_DEVICE_H_
#define MTK_AUDIO_DEVICE_H_
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#if 1
#ifdef CONFIG_I2S_MMAP
#undef CONFIG_I2S_MMAP
#endif
#endif

#define HW_PTR_SHIFT (2)

#endif /* MTK_AUDIO_DEVICE_H_ */
