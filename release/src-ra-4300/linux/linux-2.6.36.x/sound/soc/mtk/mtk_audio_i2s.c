/*
 * mtk_audio_drv.c
 *
 *  Created on: 2013/8/20
 *      Author: MTK04880
 */
#include <linux/init.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35)
#include <linux/sched.h>
#endif
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,36)
#include <asm/system.h> /* cli(), *_flags */
#endif
#include <asm/uaccess.h> /* copy_from/to_user */
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <sound/core.h>
#include <linux/pci.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include "drivers/char/ralink_gdma.h"
#include "mtk_audio_driver.h"

/****************************/
/*GLOBAL VARIABLE DEFINITION*/
/****************************/
extern i2s_config_type* pi2s_config;

/****************************/
/*FUNCTION DECLRATION		*/
/****************************/
static int mtk_audio_drv_set_fmt(struct snd_soc_dai *cpu_dai,\
		unsigned int fmt);

static int  mtk_audio_drv_shutdown(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai);
static int  mtk_audio_drv_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai);
static int mtk_audio_hw_params(struct snd_pcm_substream *substream,\
				struct snd_pcm_hw_params *params,\
				struct snd_soc_dai *dai);
static int mtk_audio_drv_play_prepare(struct snd_pcm_substream *substream,struct snd_soc_dai *dai);
static int mtk_audio_drv_rec_prepare(struct snd_pcm_substream *substream,struct snd_soc_dai *dai);
static int mtk_audio_drv_hw_free(struct snd_pcm_substream *substream,struct snd_soc_dai *dai);
static int mtk_audio_drv_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *dai);


/****************************/
/*STRUCTURE DEFINITION		*/
/****************************/


static struct snd_soc_dai_ops mtk_audio_drv_dai_ops = {
	.startup = mtk_audio_drv_startup,
	.hw_params	= mtk_audio_hw_params,
	.hw_free = mtk_audio_drv_hw_free,
	//.shutdown = mtk_audio_drv_shutdown,
	.prepare = mtk_audio_drv_prepare,
	.set_fmt = mtk_audio_drv_set_fmt,
	//.set_sysclk = mtk_audio_drv_set_sysclk,
};
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)
const struct snd_soc_component_driver mtk_i2s_component = {
	.name		= "mtk-i2s",
};

struct snd_soc_dai_driver mtk_audio_drv = {
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = (SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_11025|SNDRV_PCM_RATE_12000|\
		SNDRV_PCM_RATE_16000|SNDRV_PCM_RATE_22050|SNDRV_PCM_RATE_24000|SNDRV_PCM_RATE_32000|\
		SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000),

		.formats = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
				SNDRV_PCM_FMTBIT_S24_LE),
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = (SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_11025|SNDRV_PCM_RATE_12000|\
				SNDRV_PCM_RATE_16000|SNDRV_PCM_RATE_22050|SNDRV_PCM_RATE_24000|SNDRV_PCM_RATE_32000|\
				SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000),
		.formats = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
				SNDRV_PCM_FMTBIT_S24_LE),
	},
	.ops = &mtk_audio_drv_dai_ops,
	.symmetric_rates = 1,
};
#else
struct snd_soc_dai mtk_audio_drv_dai = {
	.name = "mtk-i2s",
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = (SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_11025|SNDRV_PCM_RATE_12000|\
		SNDRV_PCM_RATE_16000|SNDRV_PCM_RATE_22050|SNDRV_PCM_RATE_24000|SNDRV_PCM_RATE_32000|\
		SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000),

		.formats = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
				SNDRV_PCM_FMTBIT_S24_LE),
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = (SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_11025|SNDRV_PCM_RATE_12000|\
				SNDRV_PCM_RATE_16000|SNDRV_PCM_RATE_22050|SNDRV_PCM_RATE_24000|SNDRV_PCM_RATE_32000|\
				SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000),
		.formats = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
				SNDRV_PCM_FMTBIT_S24_LE),
	},
	.symmetric_rates = 1,
	.ops = &mtk_audio_drv_dai_ops,
};
#endif


/****************************/
/*FUNCTION BODY				*/
/****************************/

static int mtk_audio_drv_set_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt)
{//TODO
#if 0
	unsigned long mask;
	unsigned long value;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_RIGHT_J:
		mask = KIRKWOOD_I2S_CTL_RJ;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		mask = KIRKWOOD_I2S_CTL_LJ;
		break;
	case SND_SOC_DAIFMT_I2S:
		mask = KIRKWOOD_I2S_CTL_I2S;
		break;
	default:
		return -EINVAL;
	}

	/*
	 * Set same format for playback and record
	 * This avoids some troubles.
	 */
	value = readl(priv->io+KIRKWOOD_I2S_PLAYCTL);
	value &= ~KIRKWOOD_I2S_CTL_JUST_MASK;
	value |= mask;
	writel(value, priv->io+KIRKWOOD_I2S_PLAYCTL);

	value = readl(priv->io+KIRKWOOD_I2S_RECCTL);
	value &= ~KIRKWOOD_I2S_CTL_JUST_MASK;
	value |= mask;
	writel(value, priv->io+KIRKWOOD_I2S_RECCTL);
#endif
	return 0;
}

static int mtk_audio_drv_play_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	MSG("Enter %s\n", __func__);
	i2s_config_type* rtd = (i2s_config_type*)substream->runtime->private_data;
	rtd->pss[SNDRV_PCM_STREAM_PLAYBACK] = substream;
	if(! rtd->i2sStat[SNDRV_PCM_STREAM_PLAYBACK]){
		i2s_reset_tx_param( rtd);
		// rtd->bTxDMAEnable = 1;
		i2s_tx_config( rtd);
		gdma_En_Switch(rtd,STREAM_PLAYBACK,GDMA_I2S_EN);
		i2s_clock_enable( rtd);
		i2s_tx_enable( rtd);
		rtd->i2sStat[SNDRV_PCM_STREAM_PLAYBACK] = 1;
		MSG("I2S_TXENABLE done\n");
	}

	return 0;
}

static int mtk_audio_drv_rec_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	i2s_config_type* rtd = (i2s_config_type*)substream->runtime->private_data;
	rtd->pss[SNDRV_PCM_STREAM_CAPTURE] = substream;
	if(! rtd->i2sStat[SNDRV_PCM_STREAM_CAPTURE]){
		i2s_reset_rx_param(rtd);
		//rtd->bRxDMAEnable = 1;
		i2s_rx_config(rtd);
		gdma_En_Switch(rtd,STREAM_CAPTURE,GDMA_I2S_EN);
		i2s_clock_enable(rtd);
		i2s_rx_enable(rtd);
		rtd->i2sStat[SNDRV_PCM_STREAM_CAPTURE] = 1;
	}

	//data = i2s_inw(RALINK_REG_INTENA);
	//data |=0x0400;
	//i2s_outw(RALINK_REG_INTENA, data);
	return 0;
}

static int  mtk_audio_drv_shutdown(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	//i2s_config_type* rtd = (i2s_config_type*)substream->runtime->private_data;
	MSG("%s :%d \n",__func__,__LINE__);
	return 0;
}

static int  mtk_audio_drv_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	if((! pi2s_config->i2sStat[SNDRV_PCM_STREAM_PLAYBACK]) && (! pi2s_config->i2sStat[SNDRV_PCM_STREAM_CAPTURE])){
    	MSG("func: %s:LINE:%d \n",__func__,__LINE__);
    	i2s_startup();
    	if(!pi2s_config)
    		return -1;
    	i2s_reset_config(pi2s_config);
    }
	substream->runtime->private_data = pi2s_config;
	return 0;
}
static int mtk_audio_hw_params(struct snd_pcm_substream *substream,\
				struct snd_pcm_hw_params *params,\
				struct snd_soc_dai *dai){
	unsigned int srate = 0;
	unsigned long data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	i2s_config_type* rtd = runtime->private_data;

	//printk("func: %s:LINE:%d \n",__func__,__LINE__);
	switch(params_rate(params)){
	case 8000:
		srate = 8000;
		break;
	case 16000:
		srate = 16000;
		break;
	case 32000:
		srate = 32000;
		break;
	case 44100:
		srate = 44100;
		break;
	case 48000:
		srate = 48000;
		break;
	default:
		srate = 44100;
		//MSG("audio sampling rate %u should be %d ~ %d Hz\n", (u32)params_rate(params), MIN_SRATE_HZ, MAX_SRATE_HZ);
		break;
	}
	if(srate){
		if((rtd->bRxDMAEnable != GDMA_I2S_EN) && (rtd->bTxDMAEnable != GDMA_I2S_EN)){
			rtd->srate = srate;
			MSG("set audio sampling rate to %d Hz\n", rtd->srate);
		}
	}

	return 0;
}
static int mtk_audio_drv_hw_free(struct snd_pcm_substream *substream,struct snd_soc_dai *dai){

	i2s_config_type* rtd = (i2s_config_type*)substream->runtime->private_data;
	MSG("%s %d \n",__func__,__LINE__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		if(rtd->i2sStat[SNDRV_PCM_STREAM_PLAYBACK]){
			MSG("I2S_TXDISABLE\n");
			i2s_reset_tx_param(rtd);

			//if (rtd->nTxDMAStopped<4)
			//	interruptible_sleep_on(&(rtd->i2s_tx_qh));
			//i2s_tx_disable(rtd);
			if((rtd->bRxDMAEnable==0)&&(rtd->bTxDMAEnable==0)){
				i2s_clock_disable(rtd);
			}
			rtd->i2sStat[SNDRV_PCM_STREAM_PLAYBACK] = 0;
		}
	}
	else{
		if(rtd->i2sStat[SNDRV_PCM_STREAM_CAPTURE]){
			MSG("I2S_RXDISABLE\n");
			i2s_reset_rx_param(rtd);
			if((rtd->bRxDMAEnable==0)&&(rtd->bTxDMAEnable==0)){
				i2s_clock_disable(rtd);
			}
			rtd->i2sStat[SNDRV_PCM_STREAM_CAPTURE] = 0;
		}
	}
	return 0;
}
static int mtk_audio_drv_prepare(struct snd_pcm_substream *substream,struct snd_soc_dai *dai)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return mtk_audio_drv_play_prepare(substream, dai);
	else
		return mtk_audio_drv_rec_prepare(substream, dai);

	return 0;
}
