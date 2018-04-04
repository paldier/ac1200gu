// Iverson modify : need be removed
//#define DEBUG  //autumn
// Iverson modify end

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/completion.h>
#include <linux/usb.h>
#include <linux/version.h>
#include <linux/firmware.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "btmtk_usb.h"

#define VERSION "1.0.3"

int LOAD_CODE_METHOD = BIN_FILE_METHOD;
#define LOAD_PROFILE 1
#define SUPPORT_BT_ATE 1

static struct usb_driver btmtk_usb_driver;

#ifdef LOAD_PROFILE
void btmtk_usb_load_profile(struct hci_dev *hdev);
#endif

void hex_dump(char *str, u8 *src_buf, u32 src_buf_len)
{
	unsigned char *pt;
	int x;

	pt = src_buf;
	
	printk("%s: %p, len = %d\n", str, src_buf, src_buf_len);
	
	for (x = 0; x < src_buf_len; x++) {
		if (x % 16 == 0)
			printk("0x%04x : ", x);
		printk("%02x ", ((unsigned char)pt[x]));
		if (x % 16 == 15)
			printk("\n");
	}

	printk("\n");
}

static int btmtk_usb_reset(struct usb_device *udev)
{
	int ret;

	BT_DBG("%s\n", __FUNCTION__);

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x01, DEVICE_VENDOR_REQUEST_OUT, 
						  0x01, 0x00, NULL, 0x00, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
	{
		BT_ERR("%s error(%d)\n", __FUNCTION__, ret);
		return ret;
	}

	if (ret > 0)
		ret = 0;

	return ret;
}

static int btmtk_usb_io_read32(struct btmtk_usb_data *data, u32 reg, u32 *val)
{
	u8 request = data->r_request;
	struct usb_device *udev = data->udev;
	int ret;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), request, DEVICE_VENDOR_REQUEST_IN,
						  0x0, reg, data->io_buf, 4,
						  CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		*val = 0xffffffff;
		BT_ERR("%s error(%d), reg=%x, value=%x\n", __FUNCTION__, ret, reg, *val);
		return ret;
	}

	memmove(val, data->io_buf, 4);

	*val = le32_to_cpu(*val);

	if (ret > 0)
		ret = 0;

	return ret;
}

static int btmtk_usb_io_write32(struct btmtk_usb_data *data, u32 reg, u32 val)
{
	u16 value, index;
	u8 request = data->w_request;
	struct usb_device *udev = data->udev;
	int ret;

	index = (u16)reg;
	value = val & 0x0000ffff;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), request, DEVICE_VENDOR_REQUEST_OUT,
						  value, index, NULL, 0,
						  CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
	{
		BT_ERR("%s error(%d), reg=%x, value=%x\n", __FUNCTION__, ret, reg, val);
		return ret;
	}

	index = (u16)(reg + 2);
	value = (val & 0xffff0000) >> 16;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), request, DEVICE_VENDOR_REQUEST_OUT,
						  value, index, NULL, 0,
						  CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
	{
		BT_ERR("%s error(%d), reg=%x, value=%x\n", __FUNCTION__, ret, reg, val);
		return ret;
	}

	if (ret > 0)
		ret = 0;

	return ret;
}

#ifdef SUPPORT_BT_ATE
static void usb_ate_hci_cmd_complete(struct urb *urb)
{
	if ( urb )
	{
		if ( urb->setup_packet )
    		kfree(urb->setup_packet);
    	if ( urb->transfer_buffer )
    		kfree(urb->transfer_buffer);
        urb->setup_packet = NULL;
        urb->transfer_buffer = NULL;
    }
}

static int usb_send_ate_hci_cmd(struct usb_device *udev, unsigned char* buf, int len)
{
	struct urb *urb;
	struct usb_ctrlrequest	*class_request;
	unsigned char	*hci_cmd;
	unsigned int pipe;
	int err;
	int i;

	urb = usb_alloc_urb (0, GFP_ATOMIC);
	if (!urb) {
		printk ("%s: allocate usb urb failed!\n",
				__FUNCTION__);
		return -ENOMEM;
	}

	class_request = kmalloc(sizeof(struct usb_ctrlrequest), GFP_ATOMIC);
	if (!class_request) {
	    usb_free_urb(urb);
		printk ("%s: allocate class request failed!\n",
				__FUNCTION__);
		return -ENOMEM;
	}
	
	hci_cmd = kmalloc(len, GFP_ATOMIC);
	if (!hci_cmd) {
	    usb_free_urb(urb);
	    kfree(class_request);
		printk ("%s: allocate hci_cmd failed!\n",
				__FUNCTION__);
		return -ENOMEM;
	}

    for ( i = 0 ; i < len ; i++ )
    {
        hci_cmd[i] = buf[i];
    }

	class_request->bRequestType = USB_TYPE_CLASS;
	class_request->bRequest = 0;
	class_request->wIndex = 0;
	class_request->wValue = 0;
	class_request->wLength = len;

	pipe = usb_sndctrlpipe(udev, 0x0);

	usb_fill_control_urb(urb, udev, pipe, (void *)class_request, 
			hci_cmd, len, 
			usb_ate_hci_cmd_complete, udev);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		kfree(urb->setup_packet);
		kfree(urb->transfer_buffer);
	}
	else {
		usb_mark_last_busy(udev);
	}

	usb_free_urb(urb);
	return err;
}
#endif  // SUPPORT_BT_ATE

static int btmtk_usb_switch_iobase(struct btmtk_usb_data *data, int base)
{
	int ret = 0;

	switch (base) {
	case SYSCTL:
		data->w_request = 0x42;
		data->r_request = 0x47;
		break;
	case WLAN:
		data->w_request = 0x02;
		data->r_request = 0x07;
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static void btmtk_usb_cap_init(struct btmtk_usb_data *data)
{
	btmtk_usb_io_read32(data, 0x00, &data->chip_id);

	BT_DBG ("chip id = %x\n", data->chip_id);

	if (is_mt7630(data) || is_mt7650(data)) {
		data->need_load_fw = 1;
		data->need_load_rom_patch = 0;
		data->fw_header_image = NULL;//mt7650_fw;
		data->fw_bin_file_name = "mtk/mt7650.bin";
		data->fw_len = 0;//sizeof(mt7650_fw);
	} else if(is_mt7632(data) || is_mt7662(data)) {
		data->need_load_fw = 0;
		data->need_load_rom_patch = 1;
		data->rom_patch_header_image = NULL;//mt7662_rom_patch;
		if ( (data->chip_id & 0xf) < 0x2 )
        {
            /* Iverson 20140423 - 0:E1, 1:E2 */
    		data->rom_patch_bin_file_name = "mt7662_patch_e1_hdr.bin";
        }
        else if ( (data->chip_id & 0xf) < 0x5 )
        {
            /* Iverson 20140423 - 2:E3, 3:E4, 4:E5 */
    		data->rom_patch_bin_file_name = "mt7662_patch_e3_hdr.bin";
        }
        else
        {
            BT_ERR("unknown rom patch bin file name\n");
        }
		data->rom_patch_offset = 0x90000;
		data->rom_patch_len = 0;//sizeof(mt7662_rom_patch);
	} else
		BT_ERR("unknow chip(%x)\n", data->chip_id);
}

u16 checksume16(u8 *pData, int len)
{
	int sum = 0;

	while (len > 1) {
		sum += *((u16*)pData);

		pData = pData + 2;
		
		if (sum & 0x80000000) 
			sum = (sum & 0xFFFF) + (sum >> 16);

		len -= 2;
	}

	if (len)
		sum += *((u8*)pData);

	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}

	return ~sum;
}

static int btmtk_usb_chk_crc(struct btmtk_usb_data *data, u32 checksum_len)
{
	int ret = 0;
	struct usb_device *udev = data->udev;
	
	BT_DBG("%s\n", __FUNCTION__);

	memmove(data->io_buf, &data->rom_patch_offset, 4);
	memmove(&data->io_buf[4], &checksum_len, 4);

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x1, DEVICE_VENDOR_REQUEST_OUT,
						  0x20, 0x00, data->io_buf, 8,
						  CONTROL_TIMEOUT_JIFFIES);
	
	if (ret < 0) {
		BT_ERR("%s error(%d)\n", __FUNCTION__, ret);
	}

	return ret;
}

static u16 btmtk_usb_get_crc(struct btmtk_usb_data *data)
{
	int ret = 0;
	struct usb_device *udev = data->udev;
	u16 crc, count = 0;

	BT_DBG("%s\n", __FUNCTION__);

	while (1) {
		ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), 0x01, DEVICE_VENDOR_REQUEST_IN,
						 0x21, 0x00, data->io_buf, 2,
						  CONTROL_TIMEOUT_JIFFIES);
	
		if (ret < 0) {
			crc = 0xFFFF;
			BT_ERR("%s error(%d)\n", __FUNCTION__, ret);
		}

		memmove(&crc, data->io_buf, 2);

		crc = le16_to_cpu(crc);
	
		if (crc != 0xFFFF)
			break;

		mdelay(100);
	
		if (count++ > 100) {
			BT_ERR("Query CRC over %d times\n", count);
			break;
		}
	}

	return crc;
}

static int btmtk_usb_reset_wmt(struct btmtk_usb_data *data)
{
	int ret = 0;
	
	/* reset command */
	u8 cmd[8] = {0x6F, 0xFC, 0x05, 0x01, 0x07, 0x01, 0x00, 0x04};

	memmove(data->io_buf, cmd, 8);

	BT_DBG("%s\n", __FUNCTION__);

	ret = usb_control_msg(data->udev, usb_sndctrlpipe(data->udev, 0), 0x01, 
								DEVICE_CLASS_REQUEST_OUT, 0x12, 0x00, data->io_buf, 8, CONTROL_TIMEOUT_JIFFIES);

	if (ret)
		BT_ERR("%s:(%d)\n", __FUNCTION__, ret);

	return ret;
}

static void load_code_from_bin(unsigned char **image, char *bin_name, struct device *dev, u32 *code_len)
{
	const struct firmware *fw_entry;

	if (request_firmware(&fw_entry, bin_name, dev) != 0) {
		*image = NULL;
		return;
	}

	*image = kmalloc(fw_entry->size, GFP_ATOMIC);
	memcpy(*image, fw_entry->data, fw_entry->size);
	*code_len = fw_entry->size;

	release_firmware(fw_entry);
}

static void load_rom_patch_complete(struct urb *urb)
{

	struct completion *sent_to_mcu_done = (struct completion *)urb->context;

	complete(sent_to_mcu_done);
}

static int btmtk_usb_load_rom_patch(struct btmtk_usb_data *data)
{
	u32 loop = 0;
	u32 value;
	s32 sent_len;
	int ret = 0, total_checksum = 0;
	struct urb *urb;
	u32 patch_len = 0;
	u32 cur_len = 0;
	dma_addr_t data_dma;
	struct completion sent_to_mcu_done;
	int first_block = 1;
	unsigned char phase;
	void *buf;
	char *pos;
	unsigned int pipe = usb_sndbulkpipe(data->udev, 
										data->bulk_tx_ep->bEndpointAddress);

load_patch_protect:
	btmtk_usb_switch_iobase(data, WLAN);
	btmtk_usb_io_read32(data, SEMAPHORE_03, &value);
	loop++;

	if (((value & 0x01) == 0x00) && (loop < 600)) {
		mdelay(1);
		goto load_patch_protect;
	}
	
	btmtk_usb_switch_iobase(data, SYSCTL);

	btmtk_usb_io_write32(data, 0x1c, 0x30);
	
	btmtk_usb_switch_iobase(data, WLAN);
	
	/* check ROM patch if upgrade */
	btmtk_usb_io_read32(data, COM_REG0, &value);

	if ((value & 0x02) == 0x02)
		goto error0;
	
	urb = usb_alloc_urb(0, GFP_ATOMIC);

	if (!urb) {
		ret = -ENOMEM;
		goto error0;
	}

	buf = usb_alloc_coherent(data->udev, UPLOAD_PATCH_UNIT, GFP_ATOMIC, &data_dma);

	if (!buf) {
		ret = -ENOMEM;
		goto error1;
	}

	pos = buf;

	if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
		load_code_from_bin(&data->rom_patch, data->rom_patch_bin_file_name, &data->udev->dev, &data->rom_patch_len);
	} else {
	    BT_ERR("%s:HEADER_METHOD not supported\n", __FUNCTION__);
		//data->rom_patch = data->rom_patch_header_image;
	}
	
	if (!data->rom_patch) {
		if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
			BT_ERR("%s:please assign a rom patch(/lib/firmware/%s)\n", 
				__FUNCTION__, data->rom_patch_bin_file_name);
		} else {
			BT_ERR("%s:please assign a rom patch\n", __FUNCTION__);
		}

		ret = -1;
		goto error2;
	}
	
	printk("BT FW Patch Build Time =");

	for (loop = 0; loop < 16; loop++)
		printk("%c", *(data->rom_patch + loop));
	
	BT_DBG("\n");

	BT_DBG("platform =");

	for (loop = 0; loop < 4; loop++)
		BT_DBG("%c", *(data->rom_patch + 16 + loop));

	BT_DBG("\n");

	BT_DBG("HW/SW version =");

	for (loop = 0; loop < 4; loop++)
		BT_DBG("%c", *(data->rom_patch + 20 + loop));

	BT_DBG("\n");

	BT_DBG("Patch version =");

	for (loop = 0; loop < 4; loop++)
		BT_DBG("%c", *(data->rom_patch + 24 + loop));
	
	BT_DBG("\n");

	BT_DBG("loading rom patch");

	init_completion(&sent_to_mcu_done);

	cur_len = 0x00;
	patch_len = data->rom_patch_len - PATCH_INFO_SIZE;
	
	/* loading rom patch */
	while (1) {
		s32 sent_len_max = UPLOAD_PATCH_UNIT - PATCH_HEADER_SIZE;
		sent_len = (patch_len - cur_len) >= sent_len_max ? sent_len_max : (patch_len - cur_len);

		BT_DBG("patch_len = %d\n", patch_len);
		BT_DBG("cur_len = %d\n", cur_len);
		BT_DBG("sent_len = %d\n", sent_len);

		if (sent_len > 0) {
			if (first_block == 1) {
				if (sent_len < sent_len_max)
					phase = PATCH_PHASE3;
				else
					phase = PATCH_PHASE1;
				first_block = 0;
			} else if (sent_len == sent_len_max) {
				phase = PATCH_PHASE2;
			} else {
				phase = PATCH_PHASE3;
			}

			/* prepare HCI header */
			pos[0] = 0x6F;
			pos[1] = 0xFC;
			pos[2] = (sent_len + 5) & 0xFF;
			pos[3] = ((sent_len + 5) >> 8) & 0xFF;

			/* prepare WMT header */
			pos[4] = 0x01;
			pos[5] = 0x01;
			pos[6] = (sent_len + 1) & 0xFF;
			pos[7] = ((sent_len + 1) >> 8) & 0xFF;

			pos[8] = phase;

			memcpy(&pos[9], data->rom_patch + PATCH_INFO_SIZE + cur_len, sent_len);
			
			printk("sent_len + PATCH_HEADER_SIZE = %d, phase = %d\n", sent_len + PATCH_HEADER_SIZE, phase);

			usb_fill_bulk_urb(urb, 
							  data->udev,
							  pipe,
							  buf,
							  sent_len + PATCH_HEADER_SIZE,
							  load_rom_patch_complete,
							  &sent_to_mcu_done);

			urb->transfer_dma = data_dma;
			urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

			ret = usb_submit_urb(urb, GFP_ATOMIC);
			
			if (ret)
				goto error2;

			if (!wait_for_completion_timeout(&sent_to_mcu_done, msecs_to_jiffies(1000))) {
				usb_kill_urb(urb);
				BT_ERR("upload rom_patch timeout\n");
				goto error2;
			}

			BT_DBG(".");

			mdelay(200);
			
			cur_len += sent_len;

		} else {
			break;
		}
	}
	
	total_checksum = checksume16(data->rom_patch + PATCH_INFO_SIZE, patch_len);
	 
	BT_DBG("Send checksum req..\n");

	btmtk_usb_chk_crc(data, patch_len);
	
	mdelay(20);

	if (total_checksum != btmtk_usb_get_crc(data)) {
		BT_ERR("checksum fail!, local(0x%x) <> fw(0x%x)\n", total_checksum, btmtk_usb_get_crc(data));
		ret = -1;
		goto error2;
	}

	mdelay(20);

	ret = btmtk_usb_reset_wmt(data);

	mdelay(20);

error2:
	usb_free_coherent(data->udev, UPLOAD_PATCH_UNIT, buf, data_dma);
error1:
	usb_free_urb(urb);
error0:
	btmtk_usb_io_write32(data, SEMAPHORE_03, 0x1);
	return ret;
}


static int load_fw_iv(struct btmtk_usb_data *data)
{
	int ret;
	struct usb_device *udev = data->udev;
	char *buf = kmalloc(64, GFP_ATOMIC);

	memmove(buf, data->fw_image + 32, 64);

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x01,
						  DEVICE_VENDOR_REQUEST_OUT, 0x12, 0x0, buf, 64,
						  CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		BT_ERR("%s error(%d) step4\n", __FUNCTION__, ret);
		kfree(buf);
		return ret;
	}

	if (ret > 0)
		ret = 0;

	kfree(buf);

	return ret;
}

static void load_fw_complete(struct urb *urb)
{

	struct completion *sent_to_mcu_done = (struct completion *)urb->context;

	complete(sent_to_mcu_done);
}

static int btmtk_usb_load_fw(struct btmtk_usb_data *data)
{
	struct usb_device *udev = data->udev;
	struct urb *urb;
	void *buf;
	u32 cur_len = 0;
	u32 packet_header = 0;
	u32 value;
	u32 ilm_len = 0, dlm_len = 0; 
	u16 fw_ver, build_ver;
	u32 loop = 0;
	dma_addr_t data_dma;
	int ret = 0, sent_len;
	struct completion sent_to_mcu_done;
	unsigned int pipe = usb_sndbulkpipe(data->udev, 
										data->bulk_tx_ep->bEndpointAddress);

	BT_DBG("bulk_tx_ep = %x\n", data->bulk_tx_ep->bEndpointAddress);

loadfw_protect:
	btmtk_usb_switch_iobase(data, WLAN);
	btmtk_usb_io_read32(data, SEMAPHORE_00, &value);
	loop++;

	if (((value & 0x1) == 0) && (loop < 10000))
		goto loadfw_protect;

	/* check MCU if ready */
	btmtk_usb_io_read32(data, COM_REG0, &value);

	if ((value & 0x01)== 0x01)
		goto error0;
	
	/* Enable MPDMA TX and EP2 load FW mode */
	btmtk_usb_io_write32(data, 0x238, 0x1c000000);

	btmtk_usb_reset(udev);
	mdelay(100);
	
	if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
		load_code_from_bin(&data->fw_image, data->fw_bin_file_name, &data->udev->dev, &data->fw_len);
	} else {
		data->fw_image = data->fw_header_image;
	}
	
	if (!data->fw_image) {
		if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
			BT_ERR("%s:please assign a fw(/lib/firmware/%s)\n", 
				__FUNCTION__, data->fw_bin_file_name);
		} else {
			BT_ERR("%s:please assign a fw\n", __FUNCTION__);
		}

		ret = -1;
		goto error0;
	}

	ilm_len = (*(data->fw_image + 3) << 24) | (*(data->fw_image + 2) << 16) |
				(*(data->fw_image +1) << 8) | (*data->fw_image);

	dlm_len = (*(data->fw_image + 7) << 24) | (*(data->fw_image + 6) << 16) |
				(*(data->fw_image + 5) << 8) | (*(data->fw_image + 4));

	fw_ver = (*(data->fw_image + 11) << 8) | (*(data->fw_image + 10));

	build_ver = (*(data->fw_image + 9) << 8) | (*(data->fw_image + 8));

	BT_DBG("fw version:%d.%d.%02d ", (fw_ver & 0xf000) >> 8,
									(fw_ver & 0x0f00) >> 8,
									(fw_ver & 0x00ff));

	BT_DBG("build:%x\n", build_ver);

	BT_DBG("build Time =");

	for (loop = 0; loop < 16; loop++)
		BT_DBG("%c", *(data->fw_image + 16 + loop));

	BT_DBG("\n");

	BT_DBG("ILM length = %d(bytes)\n", ilm_len);
	BT_DBG("DLM length = %d(bytes)\n", dlm_len);

	btmtk_usb_switch_iobase(data, SYSCTL);

	/* U2M_PDMA rx_ring_base_ptr */
	btmtk_usb_io_write32(data, 0x790, 0x400230);

	/* U2M_PDMA rx_ring_max_cnt */
	btmtk_usb_io_write32(data, 0x794, 0x1);

	/* U2M_PDMA cpu_idx */
	btmtk_usb_io_write32(data, 0x798, 0x1);

	/* U2M_PDMA enable */
	btmtk_usb_io_write32(data, 0x704, 0x44);

	urb = usb_alloc_urb(0, GFP_ATOMIC);

	if (!urb) {
		ret = -ENOMEM;
		goto error1;
	}

	buf = usb_alloc_coherent(udev, 14592, GFP_ATOMIC, &data_dma);

	if (!buf) {
		ret = -ENOMEM;
		goto error2;
	}

	BT_DBG("loading fw");

	init_completion(&sent_to_mcu_done);

	btmtk_usb_switch_iobase(data, SYSCTL);

	cur_len = 0x40;

	/* Loading ILM */
	while (1) {
		sent_len = (ilm_len - cur_len) >= 14336 ? 14336 : (ilm_len - cur_len);

		if (sent_len > 0) {
			packet_header &= ~(0xffffffff);
			packet_header |= (sent_len << 16);
			packet_header = cpu_to_le32(packet_header);

			memmove(buf, &packet_header, 4);
			memmove(buf + 4, data->fw_image + 32 + cur_len, sent_len);

			/* U2M_PDMA descriptor */
			btmtk_usb_io_write32(data, 0x230, cur_len);

			while ((sent_len % 4) != 0) {
				sent_len++;
			}

			/* U2M_PDMA length */
			btmtk_usb_io_write32(data, 0x234, sent_len << 16);

			usb_fill_bulk_urb(urb, 
							  udev,
							  pipe,
							  buf,
							  sent_len + 4,
							  load_fw_complete,
							  &sent_to_mcu_done);
							  
			urb->transfer_dma = data_dma;
			urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

			ret = usb_submit_urb(urb, GFP_ATOMIC);
			
			if (ret)
				goto error3;

			if (!wait_for_completion_timeout(&sent_to_mcu_done, msecs_to_jiffies(1000))) {
				usb_kill_urb(urb);
				BT_ERR("upload ilm fw timeout\n");
				goto error3;
			}

			BT_DBG(".");

			mdelay(200);

			cur_len += sent_len;
		} else {
			break;
		}
	}
	
	init_completion(&sent_to_mcu_done);
	cur_len = 0x00;
	
	/* Loading DLM */
	while (1) {
		sent_len = (dlm_len - cur_len) >= 14336 ? 14336 : (dlm_len - cur_len);

		if (sent_len > 0) {
			packet_header &= ~(0xffffffff);
			packet_header |= (sent_len << 16);
			packet_header = cpu_to_le32(packet_header);

			memmove(buf, &packet_header, 4);
			memmove(buf + 4, data->fw_image + 32 + ilm_len + cur_len, sent_len);
			
			/* U2M_PDMA descriptor */
			btmtk_usb_io_write32(data, 0x230, 0x80000 + cur_len);

			while ((sent_len % 4) != 0) {
				BT_DBG("sent_len is not divided by 4\n");
				sent_len++;
			}
			
			/* U2M_PDMA length */
			btmtk_usb_io_write32(data, 0x234, sent_len << 16);

			usb_fill_bulk_urb(urb, 
							  udev,
							  pipe,
							  buf,
							  sent_len + 4,
							  load_fw_complete,
							  &sent_to_mcu_done);
			
			urb->transfer_dma = data_dma;
			urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

			ret = usb_submit_urb(urb, GFP_ATOMIC);
			
			if (ret)
				goto error3;

			if (!wait_for_completion_timeout(&sent_to_mcu_done, msecs_to_jiffies(1000))) {
				usb_kill_urb(urb);
				BT_ERR("upload dlm fw timeout\n");
				goto error3;
			}
			
			BT_DBG(".");

			mdelay(500);

			cur_len += sent_len;

		} else {
			break;
		}
	}
	
	/* upload 64bytes interrupt vector */
	ret = load_fw_iv(data);
	mdelay(100);

	btmtk_usb_switch_iobase(data, WLAN);

	/* check MCU if ready */
	loop = 0;

	do {
		btmtk_usb_io_read32(data, COM_REG0, &value);

		if (value == 0x01)
			break;
		
		mdelay(10);
		loop++;
	} while (loop <= 100);

	if (loop > 1000)
	{
		BT_ERR("wait for 100 times\n");
		ret = -ENODEV;
	}

error3:
	usb_free_coherent(udev, 14592, buf, data_dma);
error2:
	usb_free_urb(urb);
error1:
	/* Disbale load fw mode */
	btmtk_usb_io_read32(data, 0x238, &value);
	value = value & ~(0x10000000);
	btmtk_usb_io_write32(data,  0x238, value);
error0:
	btmtk_usb_io_write32(data, SEMAPHORE_00, 0x1);
	return ret;
}

static int inc_tx(struct btmtk_usb_data *data)
{
	unsigned long flags;
	int rv;

	spin_lock_irqsave(&data->txlock, flags);
	rv = test_bit(BTUSB_SUSPENDING, &data->flags);
	if (!rv)
		data->tx_in_flight++;
	spin_unlock_irqrestore(&data->txlock, flags);

	return rv;
}

static void btmtk_usb_intr_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif
	int err;
#ifdef SUPPORT_BT_ATE
    int is_ate_event = 0;
#endif

	BT_DBG("%s: %s urb %p status %d count %d\n", __FUNCTION__, hdev->name,
					urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

	if (urb->status == 0) {
		hdev->stat.byte_rx += urb->actual_length;
		
		//hex_dump("hci event", urb->transfer_buffer, urb->actual_length);

#ifdef SUPPORT_BT_ATE
        // check ATE cmd event
        {
            unsigned char* event_buf = urb->transfer_buffer;
            u32 event_buf_len = urb->actual_length;
            u8 matched = 0;
            int i;
            u32 Count_Tx_ACL=0;
            u32 Count_ACK=0;
            u8 PHY_RATE=0;
            if ( event_buf )
            {
                if ( event_buf[3] == 0x4D && event_buf[4] == 0xFC )
                    matched = 1;
                        
                if ( matched )
                {
                    is_ate_event = 1;
                    printk("Got ATE event result:(%d) \n    ", event_buf_len);
                    for ( i = 0 ; i < event_buf_len ; i++ )
                    {
                        printk("%02X ", event_buf[i]);
                    }
                    printk("\n");

                    Count_Tx_ACL = event_buf[6] | ((event_buf[7]<<8)&0xff00) | ((event_buf[8]<<16)&0xff0000) | ((event_buf[9]<<24)&0xff000000);
                    Count_ACK = event_buf[10] | ((event_buf[11]<<8)&0xff00) | ((event_buf[12]<<16)&0xff0000) | ((event_buf[13]<<24)&0xff000000);
                    PHY_RATE = event_buf[14];

                    printk("Count_Tx_ACL = 0x%08X\n", Count_Tx_ACL);
                    printk("Count_ACK = 0x%08X\n", Count_ACK);
                    if ( PHY_RATE == 0 )
                        printk("PHY_RATE = 1M_DM\n");
                    else if ( PHY_RATE == 1 )
                        printk("PHY_RATE = 1M_DH\n");
                    else if ( PHY_RATE == 2 )
                        printk("PHY_RATE = 2M\n");
                    else if ( PHY_RATE == 3 )
                        printk("PHY_RATE = 3M\n");
                }
            }
        }
        
        if ( is_ate_event == 0 )
#endif  // SUPPORT_BT_ATE
		if (hci_recv_fragment(hdev, HCI_EVENT_PKT,
						urb->transfer_buffer,
						urb->actual_length) < 0) {
			BT_ERR("%s corrupted event packet", hdev->name);
			hdev->stat.err_rx++;
		}
	}

	if (!test_bit(BTUSB_INTR_RUNNING, &data->flags))
		return;

	usb_mark_last_busy(data->udev);
	usb_anchor_urb(urb, &data->intr_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	
	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p failed to resubmit (%d)",
						hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static int btmtk_usb_submit_intr_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size;

	BT_DBG("%s\n", __FUNCTION__);

	if (!data->intr_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb)
		return -ENOMEM;

	size = le16_to_cpu(data->intr_ep->wMaxPacketSize);

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvintpipe(data->udev, data->intr_ep->bEndpointAddress);

	usb_fill_int_urb(urb, data->udev, pipe, buf, size,
						btmtk_usb_intr_complete, hdev,
						data->intr_ep->bInterval);

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_anchor_urb(urb, &data->intr_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p submission failed (%d)",
						hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;


}

static void btmtk_usb_bulk_in_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif
	int err;

	BT_DBG("%s:%s urb %p status %d count %d", __FUNCTION__, hdev->name,
					urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
	{
		return;
	}
	

	if (urb->status == 0) {
		hdev->stat.byte_rx += urb->actual_length;

		if (hci_recv_fragment(hdev, HCI_ACLDATA_PKT,
						urb->transfer_buffer,
						urb->actual_length) < 0) {
			BT_ERR("%s corrupted ACL packet", hdev->name);
			hdev->stat.err_rx++;
		}
	}

	if (!test_bit(BTUSB_BULK_RUNNING, &data->flags))
		return;

	usb_anchor_urb(urb, &data->bulk_anchor);
	usb_mark_last_busy(data->udev);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p failed to resubmit (%d)",
						hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static int btmtk_usb_submit_bulk_in_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size = HCI_MAX_FRAME_SIZE;

	BT_DBG("%s:%s\n", __FUNCTION__, hdev->name);

	if (!data->bulk_rx_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb)
		return -ENOMEM;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvbulkpipe(data->udev, data->bulk_rx_ep->bEndpointAddress);

	usb_fill_bulk_urb(urb, data->udev, pipe,
					buf, size, btmtk_usb_bulk_in_complete, hdev);

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_mark_last_busy(data->udev);
	usb_anchor_urb(urb, &data->bulk_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p submission failed (%d)",
						hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

static void btmtk_usb_isoc_in_complete(struct urb *urb)

{
	struct hci_dev *hdev = urb->context;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif
	int i, err;

	BT_DBG("%s: %s urb %p status %d count %d", __FUNCTION__, hdev->name,
					urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

	if (urb->status == 0) {
		for (i = 0; i < urb->number_of_packets; i++) {
			unsigned int offset = urb->iso_frame_desc[i].offset;
			unsigned int length = urb->iso_frame_desc[i].actual_length;

			if (urb->iso_frame_desc[i].status)
				continue;

			hdev->stat.byte_rx += length;

			if (hci_recv_fragment(hdev, HCI_SCODATA_PKT,
						urb->transfer_buffer + offset,
								length) < 0) {
				BT_ERR("%s corrupted SCO packet", hdev->name);
				hdev->stat.err_rx++;
			}
		}
	}

	if (!test_bit(BTUSB_ISOC_RUNNING, &data->flags))
		return;

	usb_anchor_urb(urb, &data->isoc_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p failed to resubmit (%d)",
						hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static inline void __fill_isoc_descriptor(struct urb *urb, int len, int mtu)
{
	int i, offset = 0;

	BT_DBG("len %d mtu %d", len, mtu);

	for (i = 0; i < BTUSB_MAX_ISOC_FRAMES && len >= mtu;
					i++, offset += mtu, len -= mtu) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = mtu;
	}

	if (len && i < BTUSB_MAX_ISOC_FRAMES) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = len;
		i++;
	}

	urb->number_of_packets = i;
}

static int btmtk_usb_submit_isoc_in_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size;

	BT_DBG("%s\n", __FUNCTION__);

	if (!data->isoc_rx_ep)
		return -ENODEV;

	urb = usb_alloc_urb(BTUSB_MAX_ISOC_FRAMES, mem_flags);
	if (!urb)
		return -ENOMEM;

	size = le16_to_cpu(data->isoc_rx_ep->wMaxPacketSize) *
						BTUSB_MAX_ISOC_FRAMES;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvisocpipe(data->udev, data->isoc_rx_ep->bEndpointAddress);

	usb_fill_int_urb(urb, data->udev, pipe, buf, size, btmtk_usb_isoc_in_complete,
				hdev, data->isoc_rx_ep->bInterval);

	urb->transfer_flags  = URB_FREE_BUFFER | URB_ISO_ASAP;

	__fill_isoc_descriptor(urb, size,
			le16_to_cpu(data->isoc_rx_ep->wMaxPacketSize));

	usb_anchor_urb(urb, &data->isoc_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p submission failed (%d)",
						hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

static int btmtk_usb_open(struct hci_dev *hdev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif
	int err;
	
	BT_DBG("%s\n", __FUNCTION__);

	err = usb_autopm_get_interface(data->intf);
	if (err < 0)
		return err;

	data->intf->needs_remote_wakeup = 1;
	
	if (test_and_set_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (test_and_set_bit(BTUSB_INTR_RUNNING, &data->flags))
		goto done;

	err = btmtk_usb_submit_intr_urb(hdev, GFP_KERNEL);
	if (err < 0)
		goto failed;

	err = btmtk_usb_submit_bulk_in_urb(hdev, GFP_KERNEL);
	if (err < 0) {
		usb_kill_anchored_urbs(&data->intr_anchor);
		goto failed;
	}

	set_bit(BTUSB_BULK_RUNNING, &data->flags);
	btmtk_usb_submit_bulk_in_urb(hdev, GFP_KERNEL);

#ifdef LOAD_PROFILE
    btmtk_usb_load_profile(hdev);
#endif

done:
	usb_autopm_put_interface(data->intf);
	return 0;

failed:
	clear_bit(BTUSB_INTR_RUNNING, &data->flags);
	clear_bit(HCI_RUNNING, &hdev->flags);
	usb_autopm_put_interface(data->intf);
	return err;
}

static void btmtk_usb_stop_traffic(struct btmtk_usb_data *data)
{
	BT_DBG("%s\n", __FUNCTION__);

	usb_kill_anchored_urbs(&data->intr_anchor);
	usb_kill_anchored_urbs(&data->bulk_anchor);
	usb_kill_anchored_urbs(&data->isoc_anchor);
}

static int btmtk_usb_close(struct hci_dev *hdev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif
	int err;

	BT_DBG("%s\n", __FUNCTION__);

	if (!test_and_clear_bit(HCI_RUNNING, &hdev->flags))
		return 0;

	cancel_work_sync(&data->work);
	cancel_work_sync(&data->waker);

	clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
	clear_bit(BTUSB_BULK_RUNNING, &data->flags);
	clear_bit(BTUSB_INTR_RUNNING, &data->flags);

	btmtk_usb_stop_traffic(data);

	err = usb_autopm_get_interface(data->intf);
	if (err < 0)
		goto failed;

	data->intf->needs_remote_wakeup = 0;
	usb_autopm_put_interface(data->intf);

failed:
	usb_scuttle_anchored_urbs(&data->deferred);
	return 0;
}

static int btmtk_usb_flush(struct hci_dev *hdev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif

	BT_DBG("%s\n", __FUNCTION__);

	usb_kill_anchored_urbs(&data->tx_anchor);

	return 0;
}

static void btmtk_usb_tx_complete(struct urb *urb)
{
	struct sk_buff *skb = urb->context;
	struct hci_dev *hdev = (struct hci_dev *)skb->dev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif

	BT_DBG("%s: %s urb %p status %d count %d\n", __FUNCTION__, hdev->name,
					urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (!urb->status)
		hdev->stat.byte_tx += urb->transfer_buffer_length;
	else
		hdev->stat.err_tx++;

done:
	spin_lock(&data->txlock);
	data->tx_in_flight--;
	spin_unlock(&data->txlock);

	kfree(urb->setup_packet);

	kfree_skb(skb);
}

static void btmtk_usb_isoc_tx_complete(struct urb *urb)
{
	struct sk_buff *skb = urb->context;
	struct hci_dev *hdev = (struct hci_dev *) skb->dev;

	BT_DBG("%s: %s urb %p status %d count %d", __FUNCTION__, hdev->name,
					urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (!urb->status)
		hdev->stat.byte_tx += urb->transfer_buffer_length;
	else
		hdev->stat.err_tx++;

done:
	kfree(urb->setup_packet);

	kfree_skb(skb);
}

static int btmtk_usb_send_frame(struct sk_buff *skb)
{
	struct hci_dev *hdev = (struct hci_dev *)skb->dev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif
	struct usb_ctrlrequest *dr;
	struct urb *urb;
	unsigned int pipe;
	int err;

	BT_DBG("%s\n", __FUNCTION__);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return -EBUSY;

	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
		urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		dr = kmalloc(sizeof(*dr), GFP_ATOMIC);
		if (!dr) {
			usb_free_urb(urb);
			return -ENOMEM;
		}

		dr->bRequestType = data->cmdreq_type;
		dr->bRequest     = 0;
		dr->wIndex       = 0;
		dr->wValue       = 0;
		dr->wLength      = __cpu_to_le16(skb->len);

		pipe = usb_sndctrlpipe(data->udev, 0x00);
		
		if (test_bit(HCI_RUNNING, &hdev->flags)) {
			u16 op_code;
			memcpy(&op_code, skb->data, 2);
			BT_DBG("ogf = %x\n", (op_code & 0xfc00) >> 10);
			BT_DBG("ocf = %x\n", op_code & 0x03ff);
			//hex_dump("hci command", skb->data, skb->len);

		}

		usb_fill_control_urb(urb, data->udev, pipe, (void *) dr,
				skb->data, skb->len, btmtk_usb_tx_complete, skb);

		hdev->stat.cmd_tx++;
		break;

	case HCI_ACLDATA_PKT:
		if (!data->bulk_tx_ep)
			return -ENODEV;

		urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		pipe = usb_sndbulkpipe(data->udev,
					data->bulk_tx_ep->bEndpointAddress);

		usb_fill_bulk_urb(urb, data->udev, pipe,
				skb->data, skb->len, btmtk_usb_tx_complete, skb);

		hdev->stat.acl_tx++;
		BT_DBG("HCI_ACLDATA_PKT:\n");
		break;

	case HCI_SCODATA_PKT:
		if (!data->isoc_tx_ep || hdev->conn_hash.sco_num < 1)
			return -ENODEV;

		urb = usb_alloc_urb(BTUSB_MAX_ISOC_FRAMES, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		pipe = usb_sndisocpipe(data->udev,
					data->isoc_tx_ep->bEndpointAddress);

		usb_fill_int_urb(urb, data->udev, pipe,
				skb->data, skb->len, btmtk_usb_isoc_tx_complete,
				skb, data->isoc_tx_ep->bInterval);

		urb->transfer_flags  = URB_ISO_ASAP;

		__fill_isoc_descriptor(urb, skb->len,
				le16_to_cpu(data->isoc_tx_ep->wMaxPacketSize));

		hdev->stat.sco_tx++;
		BT_DBG("HCI_SCODATA_PKT:\n");
		goto skip_waking;

	default:
		return -EILSEQ;
	}

	err = inc_tx(data);

	if (err) {
		usb_anchor_urb(urb, &data->deferred);
		schedule_work(&data->waker);
		err = 0;
		goto done;
	}

skip_waking:
	usb_anchor_urb(urb, &data->tx_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p submission failed (%d)",
						hdev->name, urb, -err);
		kfree(urb->setup_packet);
		usb_unanchor_urb(urb);
	} else {
		usb_mark_last_busy(data->udev);
	}

done:
	usb_free_urb(urb);
	return err;
}

#ifdef LOAD_PROFILE
static void btmtk_usb_ctrl_complete(struct urb *urb)
{
    BT_DBG("btmtk_usb_ctrl_complete\n");
    kfree(urb->setup_packet);
    kfree(urb->transfer_buffer);
}

static int btmtk_usb_submit_ctrl_urb(struct hci_dev *hdev, char* buf, int length)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif
	struct usb_ctrlrequest *setup_packet;
	struct urb *urb;
	unsigned int pipe;
	char* send_buf;
	int err;

	BT_DBG("btmtk_usb_submit_ctrl_urb, length=%d\n", length);

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb)
	{
    	printk("btmtk_usb_submit_ctrl_urb error1\n");
		return -ENOMEM;
    }

    send_buf = kmalloc(length, GFP_ATOMIC);
	if (!send_buf) 
	{
    	printk("btmtk_usb_submit_ctrl_urb error2\n");
    	usb_free_urb(urb);
	    return -ENOMEM;
	}
	memcpy(send_buf, buf, length);

	setup_packet = kmalloc(sizeof(*setup_packet), GFP_ATOMIC);
	if (!setup_packet) 
	{
    	printk("btmtk_usb_submit_ctrl_urb error3\n");
		usb_free_urb(urb);
		kfree(send_buf);
		return -ENOMEM;
	}

	setup_packet->bRequestType = data->cmdreq_type;
	setup_packet->bRequest     = 0;
	setup_packet->wIndex       = 0;
	setup_packet->wValue       = 0;
	setup_packet->wLength      = __cpu_to_le16(length);

	pipe = usb_sndctrlpipe(data->udev, 0x00);
	
	usb_fill_control_urb(urb, data->udev, pipe, (void *) setup_packet,
			send_buf, length, btmtk_usb_ctrl_complete, hdev);

	usb_anchor_urb(urb, &data->tx_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) 
	{
		if (err != -EPERM && err != -ENODEV)
			printk("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		kfree(urb->setup_packet);
		usb_unanchor_urb(urb);
	} 
	else 
	{
		usb_mark_last_busy(data->udev);
	}

	usb_free_urb(urb);
	return err;
}

int _ascii_to_int(char buf)
{
    switch ( buf )
    {
        case 'a':
        case 'A':
            return 10;
        case 'b':
        case 'B':
            return 11;
        case 'c':
        case 'C':
            return 12;
        case 'd':
        case 'D':
            return 13;
        case 'e':
        case 'E':
            return 14;
        case 'f':
        case 'F':
            return 15;
        default:
            return buf-'0';
    }
}
void btmtk_usb_load_profile(struct hci_dev *hdev)
{
    mm_segment_t old_fs;
    struct file *file = NULL;
    unsigned char *buf;
    unsigned char target_buf[64+4]={0};
    int i=0;
    int j=4;
    
    old_fs = get_fs();
    set_fs(KERNEL_DS);

    file = filp_open("/etc/Wireless/RT2870STA/BT_CONFIG.dat", O_RDONLY, 0);
    if (IS_ERR(file)) 
    {
        printk("could not open profile : /etc/Wireless/RT2870STA/BT_CONFIG.dat, skip...\n");
        set_fs(old_fs);
        return;
    }

    buf = kmalloc(256, GFP_ATOMIC);
    if (!buf) 
    {
        printk("malloc error when parsing /etc/Wireless/RT2870STA/BT_CONFIG.dat, exiting...\n");
        filp_close(file, NULL);
        set_fs(old_fs);
        return;
    }

    memset(buf, 0, 256);
    file->f_op->read(file, buf, 255, &file->f_pos);
    
    for ( i = 0 ; i < 255 ; i++ )
    {
        if ( buf[i] == '\r' )
            continue;
        if ( buf[i] == '\n' )
            continue;
        if ( buf[i] == 0 )
            break;
        if ( buf[i] == '0' && buf[i+1] == 'x' )
        {
            i+=1;
            continue;
        }

        {
            if ( buf[i+1] == '\r' || buf[i+1] == '\n' || buf[i+1] == 0 )
            {
                target_buf[j] = _ascii_to_int(buf[i]);
                j++;
            }
            else
            {
                target_buf[j] = _ascii_to_int(buf[i])<<4 | _ascii_to_int(buf[i+1]);
                j++;
                i++;
            }
        }
    }
    kfree(buf);
    filp_close(file, NULL);
    set_fs(old_fs);

    // Send to dongle
    {
        target_buf[0] = 0xc3;
        target_buf[1] = 0xfc;
        target_buf[2] = j-4+1;
        target_buf[3] = 0x01;

        printk("Profile Configuration : \n");
        for ( i = 0 ; i < j ; i++ )
        {
            printk("    0x%02X\n", target_buf[i]);
        }
        
        btmtk_usb_submit_ctrl_urb(hdev, target_buf, j);
    }
}
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 3, 0)
static void btmtk_usb_destruct(struct hci_dev *hdev)
{
//	struct btmtk_usb_data *data = hdev->driver_data;
}
#endif

static void btmtk_usb_notify(struct hci_dev *hdev, unsigned int evt)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif

	BT_DBG("%s evt %d", hdev->name, evt);

	if (hdev->conn_hash.sco_num != data->sco_num) {
		data->sco_num = hdev->conn_hash.sco_num;
		schedule_work(&data->work);
	}
}

#ifdef SUPPORT_BT_ATE
static int btmtk_usb_ioctl(struct hci_dev *hdev, unsigned int cmd, unsigned long arg)
{
#define ATE_TRIGGER 	_IOW('H', 300, int)
#define ATE_PARAM_LEN	_IOW('H', 301, int)
#define ATE_PARAM_0	    _IOW('H', 302, unsigned char)
#define ATE_PARAM_1	    _IOW('H', 303, unsigned char)
#define ATE_PARAM_2 	_IOW('H', 304, unsigned char)
#define ATE_PARAM_3 	_IOW('H', 305, unsigned char)
#define ATE_PARAM_4 	_IOW('H', 306, unsigned char)
#define ATE_PARAM_5 	_IOW('H', 307, unsigned char)
#define ATE_PARAM_6 	_IOW('H', 308, unsigned char)
#define ATE_PARAM_7 	_IOW('H', 309, unsigned char)
#define ATE_PARAM_8 	_IOW('H', 310, unsigned char)
#define ATE_PARAM_9 	_IOW('H', 311, unsigned char)
#define ATE_PARAM_10 	_IOW('H', 312, unsigned char)
#define ATE_PARAM_11	_IOW('H', 313, unsigned char)
#define ATE_PARAM_12	_IOW('H', 314, unsigned char)
#define ATE_PARAM_13	_IOW('H', 315, unsigned char)
#define ATE_PARAM_14	_IOW('H', 316, unsigned char)
#define ATE_PARAM_15	_IOW('H', 317, unsigned char)
#define ATE_PARAM_16	_IOW('H', 318, unsigned char)
#define ATE_PARAM_17	_IOW('H', 319, unsigned char)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif

    static char cmd_str[32]={0};
    static int cmd_len=0;

    switch ( cmd )
    {
        case ATE_TRIGGER: 
        {
            int i;
            printk("Send ATE cmd string:(%d)\n", cmd_len);

            for ( i = 0 ; i < cmd_len ; i++ )
            {
                if ( i==8 )
                    printk("\n");
                    
                if ( i==0 || i == 8 )
                    printk("    ");
                    
                printk("%02X ", (unsigned char)cmd_str[i]);
            }
            printk("\n");

            usb_send_ate_hci_cmd(data->udev, cmd_str, cmd_len);
            
            break;
        }
        case ATE_PARAM_LEN: 
            cmd_len = arg&0xff;
            break;
        case ATE_PARAM_0:
            cmd_str[0] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_1:
            cmd_str[1] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_2:
            cmd_str[2] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_3:
            cmd_str[3] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_4:
            cmd_str[4] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_5:
            cmd_str[5] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_6:
            cmd_str[6] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_7:
            cmd_str[7] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_8:
            cmd_str[8] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_9:
            cmd_str[9] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_10:
            cmd_str[10] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_11:
            cmd_str[11] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_12:
            cmd_str[12] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_13:
            cmd_str[13] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_14:
            cmd_str[14] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_15:
            cmd_str[15] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_16:
            cmd_str[16] = (unsigned char)(arg&0xff);
            break;
        case ATE_PARAM_17:
            cmd_str[17] = (unsigned char)(arg&0xff);
            break;
        default : 
            break;
    }
    
    return 0;
}
#endif  // SUPPORT_BT_ATE

static inline int __set_isoc_interface(struct hci_dev *hdev, int altsetting)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
#else
	struct btmtk_usb_data *data = hdev->driver_data;
#endif
	struct usb_interface *intf = data->isoc;
	struct usb_endpoint_descriptor *ep_desc;
	int i, err;
	
	if (!data->isoc)
		return -ENODEV;

	err = usb_set_interface(data->udev, 1, altsetting);
	if (err < 0) {
		BT_ERR("%s setting interface failed (%d)", hdev->name, -err);
		return err;
	}

	data->isoc_altsetting = altsetting;

	data->isoc_tx_ep = NULL;
	data->isoc_rx_ep = NULL;

	for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
		ep_desc = &intf->cur_altsetting->endpoint[i].desc;

		if (!data->isoc_tx_ep && usb_endpoint_is_isoc_out(ep_desc)) {
			data->isoc_tx_ep = ep_desc;
			continue;
		}

		if (!data->isoc_rx_ep && usb_endpoint_is_isoc_in(ep_desc)) {
			data->isoc_rx_ep = ep_desc;
			continue;
		}
	}

	if (!data->isoc_tx_ep || !data->isoc_rx_ep) {
		BT_ERR("%s invalid SCO descriptors", hdev->name);
		return -ENODEV;
	}

	return 0;
}

static void btmtk_usb_work(struct work_struct *work)
{
	struct btmtk_usb_data *data = container_of(work, struct btmtk_usb_data, work);
	struct hci_dev *hdev = data->hdev;
	int new_alts;
	int err;

	BT_DBG("%s\n", __FUNCTION__);

	if (hdev->conn_hash.sco_num > 0) {
		if (!test_bit(BTUSB_DID_ISO_RESUME, &data->flags)) {
			err = usb_autopm_get_interface(data->isoc ? data->isoc : data->intf);
			if (err < 0) {
				clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
				usb_kill_anchored_urbs(&data->isoc_anchor);
				return;
			}

			set_bit(BTUSB_DID_ISO_RESUME, &data->flags);
		}

		if (hdev->voice_setting & 0x0020) {
			static const int alts[3] = { 2, 4, 5 };
			new_alts = alts[hdev->conn_hash.sco_num - 1];
		} else {
			new_alts = hdev->conn_hash.sco_num;
		}

		if (data->isoc_altsetting != new_alts) {
			clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
			usb_kill_anchored_urbs(&data->isoc_anchor);

			if (__set_isoc_interface(hdev, new_alts) < 0)
				return;
		}

		if (!test_and_set_bit(BTUSB_ISOC_RUNNING, &data->flags)) {
			if (btmtk_usb_submit_isoc_in_urb(hdev, GFP_KERNEL) < 0)
				clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
			else
				btmtk_usb_submit_isoc_in_urb(hdev, GFP_KERNEL);
		}
	} else {
		clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
		usb_kill_anchored_urbs(&data->isoc_anchor);

		__set_isoc_interface(hdev, 0);

		if (test_and_clear_bit(BTUSB_DID_ISO_RESUME, &data->flags))
			 usb_autopm_put_interface(data->isoc ? data->isoc : data->intf);
	}
}

static void btmtk_usb_waker(struct work_struct *work)
{
	struct btmtk_usb_data *data = container_of(work, struct btmtk_usb_data, waker);
	int err;

	err = usb_autopm_get_interface(data->intf);

	if (err < 0)
		return;

	usb_autopm_put_interface(data->intf);
}

static int btmtk_usb_probe(struct usb_interface *intf,
							const struct usb_device_id *id)
{
	struct btmtk_usb_data *data;
	struct usb_endpoint_descriptor *ep_desc;
	int i, err;
	struct hci_dev *hdev;

	BT_DBG("%s\n", __FUNCTION__);

	/* interface numbers are hardcoded in the spec */
	if (intf->cur_altsetting->desc.bInterfaceNumber != 0)
		return -ENODEV;
	
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	
	if (!data)
		return -ENOMEM;

	for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
		ep_desc = &intf->cur_altsetting->endpoint[i].desc;

		if (!data->intr_ep && usb_endpoint_is_int_in(ep_desc)) {
			data->intr_ep = ep_desc;
			continue;
		}

		if (!data->bulk_tx_ep && usb_endpoint_is_bulk_out(ep_desc)) {
			data->bulk_tx_ep = ep_desc;
			continue;
		}

		if (!data->bulk_rx_ep && usb_endpoint_is_bulk_in(ep_desc)) {
			data->bulk_rx_ep = ep_desc;
			continue;
		}
	}

	if (!data->intr_ep || !data->bulk_tx_ep || !data->bulk_rx_ep) {
		kfree(data);
		return -ENODEV;
	}
	
	data->cmdreq_type = USB_TYPE_CLASS;
	
	data->udev = interface_to_usbdev(intf);
	data->intf = intf;

	spin_lock_init(&data->lock);	
	INIT_WORK(&data->work, btmtk_usb_work);
	INIT_WORK(&data->waker, btmtk_usb_waker);
	spin_lock_init(&data->txlock);
	
	init_usb_anchor(&data->tx_anchor);
	init_usb_anchor(&data->intr_anchor);
	init_usb_anchor(&data->bulk_anchor);
	init_usb_anchor(&data->isoc_anchor);
	init_usb_anchor(&data->deferred);
	
	data->io_buf = kmalloc(256, GFP_ATOMIC);
	
	btmtk_usb_switch_iobase(data, WLAN);

	btmtk_usb_cap_init(data);
	
	if (data->need_load_rom_patch) {
		err = btmtk_usb_load_rom_patch(data);

		if (err < 0) {
			kfree(data->io_buf);
			kfree(data);
			return err;
		}
	}

	if (data->need_load_fw) {
		err = btmtk_usb_load_fw(data);
	
		if (err < 0) {
			kfree(data->io_buf);
			kfree(data);
			return err;
		}
	}
	
	hdev = hci_alloc_dev();
	if (!hdev) {
		kfree(data);
		return -ENOMEM;
	}
	
	hdev->bus = HCI_USB;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	hci_set_drvdata(hdev, data);
#else
	hdev->driver_data = data;
#endif

	data->hdev = hdev;

	SET_HCIDEV_DEV(hdev, &intf->dev);

	hdev->open     = btmtk_usb_open;
	hdev->close    = btmtk_usb_close;
	hdev->flush    = btmtk_usb_flush;
	hdev->send     = btmtk_usb_send_frame;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 3, 0)
	hdev->destruct = btmtk_usb_destruct; 
#endif
	hdev->notify   = btmtk_usb_notify;
#ifdef SUPPORT_BT_ATE
	hdev->ioctl    = btmtk_usb_ioctl;
#endif
	
	/* Interface numbers are hardcoded in the specification */
	data->isoc = usb_ifnum_to_if(data->udev, 1);
	
	if (data->isoc) {
		err = usb_driver_claim_interface(&btmtk_usb_driver,
							data->isoc, data);
		if (err < 0) {
			hci_free_dev(hdev);
			kfree(data->io_buf);
			kfree(data);
			return err;
		}
	}

	err = hci_register_dev(hdev);
	if (err < 0) {
		hci_free_dev(hdev);
		kfree(data->io_buf);
		kfree(data);
		return err;
	}

	usb_set_intfdata(intf, data);

	return 0;
}

static void btmtk_usb_disconnect(struct usb_interface *intf)
{
	struct btmtk_usb_data *data = usb_get_intfdata(intf);
	struct hci_dev *hdev;
	
	BT_DBG("%s\n", __FUNCTION__);
	
	if (!data)
		return;
	
	hdev = data->hdev;

	usb_set_intfdata(data->intf, NULL);

	if (data->isoc)
		usb_set_intfdata(data->isoc, NULL);
	
	hci_unregister_dev(hdev);

	if (intf == data->isoc)
		usb_driver_release_interface(&btmtk_usb_driver, data->intf);
	else if (data->isoc)
		usb_driver_release_interface(&btmtk_usb_driver, data->isoc);

	hci_free_dev(hdev);
	kfree(data->io_buf);

	if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
		if (data->need_load_rom_patch)
			kfree(data->rom_patch);

		if (data->need_load_fw)
			kfree(data->fw_image);
	}

	kfree(data);
}

#ifdef CONFIG_PM
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
#define PMSG_IS_AUTO(msg)       (((msg).event & PM_EVENT_AUTO) != 0)
#endif
static int btmtk_usb_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct btmtk_usb_data *data = usb_get_intfdata(intf);
	
	BT_DBG("%s\n", __FUNCTION__);

	if (data->suspend_count++)
		return 0;

	spin_lock_irq(&data->txlock);
	if (!(PMSG_IS_AUTO(message) && data->tx_in_flight)) {
		set_bit(BTUSB_SUSPENDING, &data->flags);
		spin_unlock_irq(&data->txlock);
	} else {
		spin_unlock_irq(&data->txlock);
		data->suspend_count--;
		return -EBUSY;
	}

	cancel_work_sync(&data->work);

	btmtk_usb_stop_traffic(data);
	usb_kill_anchored_urbs(&data->tx_anchor);

	return 0;
}

static void play_deferred(struct btmtk_usb_data *data)
{
	struct urb *urb;
	int err;

	while ((urb = usb_get_from_anchor(&data->deferred))) {
		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err < 0)
			break;

		data->tx_in_flight++;
	}

	usb_scuttle_anchored_urbs(&data->deferred);
}

static int btmtk_usb_resume(struct usb_interface *intf)
{
	struct btmtk_usb_data *data = usb_get_intfdata(intf);
	struct hci_dev *hdev = data->hdev;
	int err = 0;	

	BT_DBG("%s\n", __FUNCTION__);

	if (--data->suspend_count)
		return 0;

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (test_bit(BTUSB_INTR_RUNNING, &data->flags)) {
		err = btmtk_usb_submit_intr_urb(hdev, GFP_NOIO);
		if (err < 0) {
			clear_bit(BTUSB_INTR_RUNNING, &data->flags);
			goto failed;
		}
	}

	if (test_bit(BTUSB_BULK_RUNNING, &data->flags)) {
		err = btmtk_usb_submit_bulk_in_urb(hdev, GFP_NOIO);
		if (err < 0) {
			clear_bit(BTUSB_BULK_RUNNING, &data->flags);
			goto failed;
		}

		btmtk_usb_submit_bulk_in_urb(hdev, GFP_NOIO);
	}

	if (test_bit(BTUSB_ISOC_RUNNING, &data->flags)) {
		if (btmtk_usb_submit_isoc_in_urb(hdev, GFP_NOIO) < 0)
			clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
		else
			btmtk_usb_submit_isoc_in_urb(hdev, GFP_NOIO);
	}

	spin_lock_irq(&data->txlock);
	play_deferred(data);
	clear_bit(BTUSB_SUSPENDING, &data->flags);
	spin_unlock_irq(&data->txlock);
	schedule_work(&data->work);
	
	return 0;

failed:
	usb_scuttle_anchored_urbs(&data->deferred);
done:
	spin_lock_irq(&data->txlock);
	clear_bit(BTUSB_SUSPENDING, &data->flags);
	spin_unlock_irq(&data->txlock);

	return err;
}
#endif

static struct usb_device_id btmtk_usb_table[] = {
	/* Mediatek MT7650 */
	{ USB_DEVICE_AND_INTERFACE_INFO(0x0e8d, 0x7650, 0xe0, 0x01, 0x01) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x0e8d, 0x7650, 0xff, 0xff, 0xff) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x0e8d, 0x7630, 0xe0, 0x01, 0x01) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x0e8d, 0x7630, 0xff, 0xff, 0xff) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x0e8d, 0x763e, 0xe0, 0x01, 0x01) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x0e8d, 0x763e, 0xff, 0xff, 0xff) },
	/* Mediatek MT662 */
	{ USB_DEVICE_AND_INTERFACE_INFO(0x0e8d, 0x7662, 0xe0, 0x01, 0x01) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0x0e8d, 0x7632, 0xe0, 0x01, 0x01) },
	{ }	/* Terminating entry */
};

static struct usb_driver btmtk_usb_driver = {
	.name		= "btmtk_usb",
	.probe		= btmtk_usb_probe,
	.disconnect	= btmtk_usb_disconnect,
#ifdef CONFIG_PM
	.suspend	= btmtk_usb_suspend,
	.resume		= btmtk_usb_resume,
#endif
	.id_table	= btmtk_usb_table,
	.supports_autosuspend = 1,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
	.disable_hub_initiated_lpm = 1,
#endif
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
module_usb_driver(btmtk_usb_driver);
#else
static int __init btmtk_usb_init(void)
{
	BT_INFO("btmtk usb driver ver %s", VERSION);

	return usb_register(&btmtk_usb_driver);
}

static void __exit btmtk_usb_exit(void)
{
	usb_deregister(&btmtk_usb_driver);
}

module_init(btmtk_usb_init);
module_exit(btmtk_usb_exit);
#endif

MODULE_DESCRIPTION("Mediatek Bluetooth USB driver ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
MODULE_FIRMWARE("mt7662_patch_e1_hdr.bin");
MODULE_FIRMWARE("mt7662_patch_e3_hdr.bin");

