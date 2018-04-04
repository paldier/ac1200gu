/*
 * mtk_audio_pcm.c
 *
 *  Created on: 2013/9/6
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
#include <linux/delay.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include "drivers/char/ralink_gdma.h"
#include "mtk_audio_driver.h"

#define GDMA_PAGE_SZ 		I2S_PAGE_SIZE
#define GDMA_PAGE_NUM 		(MAX_I2S_PAGE)
#define GDMA_TOTAL_PAGE_SZ	(I2S_PAGE_SIZE*MAX_I2S_PAGE)

extern struct tasklet_struct i2s_tx_tasklet;
extern struct tasklet_struct i2s_rx_tasklet;

dma_addr_t i2s_txdma_addr, i2s_rxdma_addr;
dma_addr_t i2s_mmap_addr[GDMA_PAGE_NUM*2];

extern int i2s_mmap_remap(struct vm_area_struct *vma, unsigned long size);
static int mtk_audio_pcm_open(struct snd_pcm_substream *substream);
static int mtk_pcm_new(struct snd_card *card,\
	struct snd_soc_dai *dai, struct snd_pcm *pcm);
static void mtk_pcm_free(struct snd_pcm *pcm);
static int mtk_audio_pcm_close(struct snd_pcm_substream *substream);
static snd_pcm_uframes_t mtk_audio_pcm_pointer(struct snd_pcm_substream *substream);
static int mtk_audio_pcm_trigger(struct snd_pcm_substream *substream, int cmd);
static int mtk_audio_pcm_mmap(struct snd_pcm_substream *substream, struct vm_area_struct *vma);
static int mtk_audio_pcm_prepare(struct snd_pcm_substream *substream);
static int mtk_audio_pcm_hw_params(struct snd_pcm_substream *substream,\
				 struct snd_pcm_hw_params *hw_params);
static int mtk_audio_pcm_copy(struct snd_pcm_substream *substream, int channel,\
		snd_pcm_uframes_t pos,void __user *buf, snd_pcm_uframes_t count);
static int mtk_audio_pcm_hw_free(struct snd_pcm_substream *substream);

static int mtk_pcm_free_dma_buffer(struct snd_pcm_substream *substream,int stream);
static int mtk_pcm_allocate_dma_buffer(struct snd_pcm_substream *substream,int stream);

static const struct snd_pcm_hardware mtk_audio_hwparam = {
#ifdef CONFIG_I2S_MMAP
	.info			= (SNDRV_PCM_INFO_INTERLEAVED |
					SNDRV_PCM_INFO_PAUSE |
					SNDRV_PCM_INFO_RESUME |
				    SNDRV_PCM_INFO_MMAP |
				    SNDRV_PCM_INFO_MMAP_VALID),
#else
	.info			= (SNDRV_PCM_INFO_INTERLEAVED |
				    SNDRV_PCM_INFO_PAUSE |
				    SNDRV_PCM_INFO_RESUME),
#endif
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.period_bytes_min	= GDMA_PAGE_SZ,
	.period_bytes_max	= GDMA_PAGE_SZ,
	.periods_min		= 1,
	.periods_max		= GDMA_PAGE_NUM,
	.buffer_bytes_max	= GDMA_TOTAL_PAGE_SZ,
};

static struct snd_pcm_ops mtk_pcm_ops = {

	.open = 	mtk_audio_pcm_open,
	.ioctl = 	snd_pcm_lib_ioctl,
	.hw_params = mtk_audio_pcm_hw_params,
	.hw_free = 	mtk_audio_pcm_hw_free,
	.trigger =	mtk_audio_pcm_trigger,
	.prepare = 	mtk_audio_pcm_prepare,
	.pointer = 	mtk_audio_pcm_pointer,
	.close = 	mtk_audio_pcm_close,
#ifdef CONFIG_I2S_MMAP
	.mmap = mtk_audio_pcm_mmap,
#endif
	.copy = mtk_audio_pcm_copy,
};
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
struct snd_soc_platform_driver mtk_soc_platform = {
	.ops		= &mtk_pcm_ops,
	.pcm_new	= mtk_pcm_new,
	.pcm_free	= mtk_pcm_free,
};
#else
struct snd_soc_platform mtk_soc_platform = {
	.name		= "mtk-dma",
	.pcm_ops	= &mtk_pcm_ops,
	.pcm_new	= mtk_pcm_new,
	.pcm_free	= mtk_pcm_free,
};
#endif

static int mtk_audio_pcm_close(struct snd_pcm_substream *substream){
	return 0;
}

static snd_pcm_uframes_t mtk_audio_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	i2s_config_type* rtd = runtime->private_data;
	int offset = 0;
	int tmp_hw_base = 0;
	int buff_frame_bond = bytes_to_frames(runtime, GDMA_PAGE_SZ);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		offset = bytes_to_frames(runtime, GDMA_PAGE_SZ*rtd->tx_r_idx);
		//printk("r:%d w:%d (%d) \n",rtd->tx_r_idx,rtd->tx_w_idx,(runtime->control->appl_ptr/buff_frame_bond)%GDMA_PAGE_NUM);
	}
	else{
		offset = bytes_to_frames(runtime, GDMA_PAGE_SZ*rtd->rx_w_idx);
		//printk("w:%d r:%d appl_ptr:%x\n",rtd->rx_w_idx,rtd->rx_r_idx,(runtime->control->appl_ptr/buff_frame_bond)%GDMA_PAGE_NUM);
	}
	return offset;
}

#if 0
static int gdma_ctrl_start(struct snd_pcm_substream *substream){

	struct snd_pcm_runtime *runtime= substream->runtime;
	i2s_config_type* rtd = runtime->private_data;

	//printk("%s:%d \n",__func__,__LINE__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		gdma_unmask_handler(GDMA_I2S_TX0);
	}
	else{
		gdma_unmask_handler(GDMA_I2S_RX0);
	}
	return 0;
}

static int gdma_ctrl_stop(struct snd_pcm_substream *substream){

	struct snd_pcm_runtime *runtime= substream->runtime;
	i2s_config_type* rtd = runtime->private_data;

	//printk("%s:%d \n",__func__,__LINE__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		tasklet_kill(&i2s_tx_tasklet);
		gdma_En_Switch(rtd,STREAM_PLAYBACK,GDMA_I2S_DIS);
	}
	else{
		rtd->bRxDMAEnable = 0;
	}
	return 0;
}
#endif

static int mtk_audio_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	i2s_config_type* rtd = (i2s_config_type*)substream->runtime->private_data;
	struct snd_pcm_runtime *runtime= substream->runtime;
	printk("trigger cmd:%s\n",(cmd==SNDRV_PCM_TRIGGER_START)?"START":\
			(cmd==SNDRV_PCM_TRIGGER_RESUME)?"RESUME":\
					(cmd==SNDRV_PCM_TRIGGER_PAUSE_RELEASE)?"PAUSE_RELEASE":\
							(cmd==SNDRV_PCM_TRIGGER_STOP)?"STOP":\
									(cmd==SNDRV_PCM_TRIGGER_SUSPEND)?"SUSPEND":\
											(cmd==SNDRV_PCM_TRIGGER_PAUSE_PUSH)?"PAUSE_PUSH":"default");

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
			rtd->bTrigger[SNDRV_PCM_STREAM_PLAYBACK] = 1;
		}
		else{
			rtd->bTrigger[SNDRV_PCM_STREAM_CAPTURE] = 1;
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
			rtd->bTrigger[SNDRV_PCM_STREAM_PLAYBACK] = 0;
		}
		else{
			rtd->bTrigger[SNDRV_PCM_STREAM_CAPTURE] = 0;
		}
		break;

	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
#if 0
		ret = gdma_ctrl_start(substream);
#else
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){

			rtd->tx_pause_en = 0;
			//gdma_En_Switch(rtd,STREAM_PLAYBACK,GDMA_I2S_EN);
			//gdma_unmask_handler(GDMA_I2S_TX0);
		}
		else{
			rtd->rx_pause_en = 0;
			//gdma_En_Switch(rtd,STREAM_CAPTURE,GDMA_I2S_EN);
			//gdma_unmask_handler(GDMA_I2S_RX0);
		}
#endif
		break;

	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
#if 0
		ret =gdma_ctrl_stop(substream);
#else
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
			//gdma_En_Switch(rtd,STREAM_PLAYBACK,GDMA_I2S_DIS);
			rtd->tx_pause_en = 1;
		}
		else{
			rtd->rx_pause_en = 1;
			//gdma_En_Switch(rtd,STREAM_CAPTURE,GDMA_I2S_DIS);
		}
#endif
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int mtk_audio_pcm_copy(struct snd_pcm_substream *substream, int channel,\
		snd_pcm_uframes_t pos,void __user *buf, snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime= substream->runtime;
	i2s_config_type* rtd = runtime->private_data;
	int tx_w_idx = 0;
	int rx_r_idx = 0;
	char *hwbuf = runtime->dma_area + frames_to_bytes(runtime, pos);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		//printk("%s \n",__func__);
#if 1
		rtd->tx_w_idx = (rtd->tx_w_idx+1)%MAX_I2S_PAGE;
		tx_w_idx = rtd->tx_w_idx;
		//printk("put TB[%d - %x] for user write\n",rtd->tx_w_idx,pos);
		copy_from_user(rtd->pMMAPTxBufPtr[tx_w_idx], (char*)buf, I2S_PAGE_SIZE);
#else
		i2s_audio_exchange(rtd,STREAM_PLAYBACK,(unsigned long)buf);
#endif
	}
	else{
#if 1
		rx_r_idx = rtd->rx_r_idx;
		rtd->rx_r_idx = (rtd->rx_r_idx+1)%MAX_I2S_PAGE;
		copy_to_user((char*)buf, rtd->pMMAPRxBufPtr[rx_r_idx], I2S_PAGE_SIZE);
#else
		i2s_audio_exchange(rtd,STREAM_CAPTURE,(unsigned long)buf);
#endif

	}

	return 0;
}

static int mtk_audio_pcm_mmap(struct snd_pcm_substream *substream, struct vm_area_struct *vma)
{
	int ret;
	unsigned long size; 

	size = vma->vm_end-vma->vm_start;
	//printk("##### Enter %s #####size :%x end:%x start:%x\n", __func__,size,vma->vm_end,vma->vm_start);
	ret = i2s_mmap_remap(vma, size);

	return ret; 
}

static int mtk_audio_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime= substream->runtime;
	i2s_config_type *rtd = (i2s_config_type*)runtime->private_data;
	//runtime->buffer_size = (GDMA_PAGE_NUM)*GDMA_PAGE_SZ;
	//runtime->boundary = (GDMA_PAGE_NUM*GDMA_PAGE_SZ)/4;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
	//printk("===== %s:%s:%d mtk_pcm_allocate_dma_buffer\n", __FILE__, __FUNCTION__, __LINE__);
		mtk_pcm_allocate_dma_buffer(substream,SNDRV_PCM_STREAM_PLAYBACK);
		if(! rtd->dmaStat[SNDRV_PCM_STREAM_PLAYBACK]){ /* TX:enLabel=1; RX:enLabel=2 */
			i2s_page_prepare(rtd,STREAM_PLAYBACK);
			tasklet_init(&i2s_tx_tasklet, i2s_tx_task, (u32)rtd);
			rtd->dmaStat[SNDRV_PCM_STREAM_PLAYBACK] = 1;
			gdma_unmask_handler(GDMA_I2S_TX0);
		}
	}
	else{
		mtk_pcm_allocate_dma_buffer(substream,SNDRV_PCM_STREAM_CAPTURE);
		i2s_page_prepare(rtd,STREAM_CAPTURE);
		if(!rtd->dmaStat[SNDRV_PCM_STREAM_CAPTURE]){ /* TX:enLabel=1; RX:enLabel=2 */
			tasklet_init(&i2s_rx_tasklet, i2s_rx_task, (u32)rtd);
			rtd->dmaStat[SNDRV_PCM_STREAM_CAPTURE] = 1;
			//gdma_ctrl_start(substream);
			gdma_unmask_handler(GDMA_I2S_RX0);
		}
	}

	return 0;
}


static int mtk_audio_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	i2s_config_type *rtd = (i2s_config_type*)runtime->private_data;
	int ret,i;
	ret = i = 0;
	//printk("%s %d \n",__func__,__LINE__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
	//	i2s_page_prepare(rtd,STREAM_PLAYBACK);
	}
	else{
	//	i2s_page_prepare(rtd,STREAM_CAPTURE);
	}

	//snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return ret;
}

static int mtk_audio_pcm_hw_free(struct snd_pcm_substream *substream)
{
	i2s_config_type* rtd = (i2s_config_type*)substream->runtime->private_data;
	printk("%s:%s:%d \n",__FILE__,__func__, __LINE__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		if(rtd->dmaStat[SNDRV_PCM_STREAM_PLAYBACK]){

			gdma_En_Switch(rtd,STREAM_PLAYBACK,GDMA_I2S_DIS);
			i2s_dma_tx_end_handle(rtd);
			tasklet_kill(&i2s_tx_tasklet);
			i2s_tx_disable(rtd);
			mtk_pcm_free_dma_buffer(substream,substream->stream);
			i2s_page_release(rtd,STREAM_PLAYBACK);
			rtd->dmaStat[SNDRV_PCM_STREAM_PLAYBACK] = 0;
		}
	}
	else{
		if(rtd->dmaStat[SNDRV_PCM_STREAM_CAPTURE]){
			tasklet_kill(&i2s_rx_tasklet);
			gdma_En_Switch(rtd,STREAM_CAPTURE,GDMA_I2S_DIS);
			i2s_dma_rx_end_handle(rtd);
			i2s_rx_disable(rtd);
			mtk_pcm_free_dma_buffer(substream,substream->stream);
			i2s_page_release(rtd,STREAM_CAPTURE);
			rtd->dmaStat[SNDRV_PCM_STREAM_CAPTURE] = 0;
		}
	}
	return 0;
}

static int mtk_pcm_free_dma_buffer(struct snd_pcm_substream *substream,
	int stream)
{

	//struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	i2s_config_type* rtd = (i2s_config_type*)substream->runtime->private_data;

	if (!buf->area)
		return 0;
	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		i2s_memPool_free(rtd,STREAM_PLAYBACK);
	else
		i2s_memPool_free(rtd,STREAM_CAPTURE);
	buf->area = NULL;
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int mtk_pcm_allocate_dma_buffer(struct snd_pcm_substream *substream,
	int stream)
{
	//printk("Enter %s\n", __func__);
	//struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	i2s_config_type* rtd = (i2s_config_type*)substream->runtime->private_data;

	if(!buf->area){
#if defined(CONFIG_I2S_MMAP)
		//printk("############## MMAP ##############\n");
		buf->dev.type = SNDRV_DMA_TYPE_DEV;
#else
		buf->dev.type = SNDRV_DMA_TYPE_UNKNOWN;
#endif
		buf->dev.dev = NULL;
		buf->private_data = NULL;
		if(stream == SNDRV_PCM_STREAM_PLAYBACK)
			buf->area = i2s_memPool_Alloc(rtd,STREAM_PLAYBACK);
		else
			buf->area = i2s_memPool_Alloc(rtd,STREAM_CAPTURE);

		if (!buf->area)
			return -ENOMEM;
		buf->bytes = GDMA_TOTAL_PAGE_SZ;
#if defined(CONFIG_I2S_MMAP)
		buf->addr = i2s_mmap_phys_addr(rtd);	
#endif
		snd_pcm_set_runtime_buffer(substream, buf);	
	}
	//printk("#### buffer size=%d ####\n", substream->runtime->buffer_size);
	return 0;
}

static int mtk_audio_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime= substream->runtime;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	int stream = substream->stream;
	int ret = 0;

	//printk("Enter %s\n", __func__);
	snd_soc_set_runtime_hwparams(substream, &mtk_audio_hwparam);
	/* ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
						SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto out;

	if(stream == SNDRV_PCM_STREAM_PLAYBACK){
		ret = mtk_pcm_allocate_dma_buffer(substream,
				SNDRV_PCM_STREAM_PLAYBACK);
	}
	else{
		ret = mtk_pcm_allocate_dma_buffer(substream,
				SNDRV_PCM_STREAM_CAPTURE);
	}
	if (ret)
		goto out;

	if(buf)
		memset(buf->area,0,sizeof(I2S_PAGE_SIZE*MAX_I2S_PAGE));

 out:
	return ret;

}



static int mtk_pcm_new(struct snd_card *card,
	struct snd_soc_dai *dai, struct snd_pcm *pcm)
{
	printk("%s:%d \n",__func__,__LINE__);

	return 0;
}

static void mtk_pcm_free(struct snd_pcm *pcm)
{

	printk("%s:%d \n",__func__,__LINE__);
}


static int __init mtk_audio_pcm_init(void)
{
printk("###########Enter %s\n", __func__);

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
	return 0;
#else
	return snd_soc_register_platform(&mtk_soc_platform);
#endif
}

static void __exit mtk_audio_pcm_exit(void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
	return;
#else
	snd_soc_unregister_platform(&mtk_soc_platform);
#endif
}

module_init(mtk_audio_pcm_init);
module_exit(mtk_audio_pcm_exit);

MODULE_AUTHOR("Atsushi Nemoto <anemo@mba.ocn.ne.jp>");
MODULE_DESCRIPTION("TXx9 ACLC Audio DMA driver");
MODULE_LICENSE("GPL");

