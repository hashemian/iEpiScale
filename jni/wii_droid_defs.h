/* Copyright (C) 2011 L. Mohammad Hashemian <m.hashemian@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef WII_DROID_DEFS_H
#define WII_DROID_DEFS_H

#define DEBUG_TAG "iEpiScaleJNI84" 
#define RPT_READ_REQ_LEN 6
#define READ_BUF_LEN 23
#define RPT_WRITE_LEN 21
#define RPT_MODE_BUF_LEN 2
#define toggle_bit(bf,b) (bf) = ((bf) & b) ? ((bf) & ~(b)) : ((bf) | (b))
#define MAX_READ_TRIAL 2
#define MAX_CAL_TRIAL 2
#define FALSE 0
#define TRUE 1

/* Flags */
#define WD_FLAG_MESG_IFC	0x01
#define WD_FLAG_CONTINUOUS	0x02
#define WD_FLAG_REPEAT_BTN	0x04
#define WD_FLAG_NONBLOCK	0x08
#define WD_FLAG_MOTIONPLUS	0x10

/* Button Mask (masks unknown bits in button bytes) */
#define BTN_MASK_0			0x1F
#define BTN_MASK_1			0x9F
#define NUNCHUK_BTN_MASK	0x03

/* Send Report flags */
#define WD_SEND_RPT_NO_RUMBLE    0x01

/* Data Read/Write flags */
#define WD_RW_EEPROM	0x00
#define WD_RW_REG		0x04

/* Report numbers */
#define RPT_LED_RUMBLE			0x11
#define RPT_RPT_MODE			0x12
#define RPT_IR_ENABLE1			0x13
#define RPT_SPEAKER_ENABLE		0x14
#define RPT_STATUS_REQ			0x15
#define RPT_WRITE				0x16
#define RPT_READ_REQ			0x17
#define RPT_SPEAKER_DATA		0x18
#define RPT_SPEAKER_MUTE		0x19
#define RPT_IR_ENABLE2			0x1A
#define RPT_STATUS				0x20
#define RPT_READ_DATA			0x21
#define RPT_WRITE_ACK			0x22
#define RPT_BTN					0x30
#define RPT_BTN_ACC				0x31
#define RPT_BTN_EXT8			0x32
#define RPT_BTN_ACC_IR12		0x33
#define RPT_BTN_EXT19			0x34
#define RPT_BTN_ACC_EXT16		0x35
#define RPT_BTN_IR10_EXT9		0x36
#define RPT_BTN_ACC_IR10_EXT6	0x37
#define RPT_EXT21				0x3D
#define RPT_BTN_ACC_IR36_1		0x3E
#define RPT_BTN_ACC_IR36_2		0x3F

/* Bluetooth magic numbers */
#define BT_TRANS_MASK		0xF0
#define BT_TRANS_HANDSHAKE	0x00
#define BT_TRANS_SET_REPORT	0x50
#define BT_TRANS_DATA		0xA0
#define BT_TRANS_DATAC		0xB0
#define BT_PARAM_MASK		0x0F

/* HANDSHAKE params */
#define BT_PARAM_SUCCESSFUL					0x00
#define BT_PARAM_NOT_READY					0x01
#define BT_PARAM_ERR_INVALID_REPORT_ID		0x02
#define BT_PARAM_ERR_UNSUPPORTED_REQUEST	0x03
#define BT_PARAM_ERR_INVALID_PARAMETER		0x04
#define BT_PARAM_ERR_UNKNOWN				0x0E
#define BT_PARAM_ERR_FATAL					0x0F

/* SET_REPORT, DATA, DATAC params */
#define BT_PARAM_INPUT		0x01
#define BT_PARAM_OUTPUT		0x02
#define BT_PARAM_FEATURE	0x03

/* IR Defs */
#define WD_IR_SRC_COUNT	4
#define WD_IR_X_MAX		1024
#define WD_IR_Y_MAX		768

/* Callback Maximum Message Count */
#define WD_MAX_MESG_COUNT	100

/* Extension Values */
#define EXT_NONE		0x2E2E
#define EXT_PARTIAL		0xFFFF
#define EXT_NUNCHUK		0x0000
#define EXT_CLASSIC		0x0101
#define EXT_BALANCE		0x0402
#define EXT_MOTIONPLUS	0x0405

/* Array Index Defs */
#define WD_X		0
#define WD_Y		1
#define WD_Z		2
#define WD_PHI		0
#define WD_THETA	1
#define WD_PSI		2

/* Wiimote port/channel/PSMs */
#define CTL_PSM	17
#define INT_PSM	19

/* Report Mode Flags */
#define WD_RPT_STATUS		0x01
#define WD_RPT_BTN			0x02
#define WD_RPT_ACC			0x04
#define WD_RPT_IR			0x08
#define WD_RPT_NUNCHUK		0x10
#define WD_RPT_CLASSIC		0x20
#define WD_RPT_BALANCE		0x40
#define WD_RPT_MOTIONPLUS	0x80
#define WD_RPT_EXT			(WD_RPT_NUNCHUK | WD_RPT_CLASSIC | \
                             WD_RPT_BALANCE | WD_RPT_MOTIONPLUS)

/* enums */
enum wd_mesg_type 
{
	WD_MESG_STATUS,
	WD_MESG_BTN,
	WD_MESG_ACC,
	WD_MESG_IR,
	WD_MESG_NUNCHUK,
	WD_MESG_CLASSIC,
	WD_MESG_BALANCE,
	WD_MESG_MOTIONPLUS,
	WD_MESG_ERROR,
	WD_MESG_UNKNOWN
};

/* RW State/Mesg */
enum rw_status 
{
	RW_IDLE,
	RW_READ,
	RW_WRITE,
	RW_CANCEL
};

enum wd_error 
{
	WD_ERROR_NONE,
	WD_ERROR_DISCONNECT,
	WD_ERROR_COMM
};

enum wd_ext_type 
{
	WD_EXT_NONE,
	WD_EXT_NUNCHUK,
	WD_EXT_CLASSIC,
	WD_EXT_BALANCE,
	WD_EXT_MOTIONPLUS,
	WD_EXT_UNKNOWN
};

/* Write Sequences */
enum write_seq_type 
{
	WRITE_SEQ_RPT,
	WRITE_SEQ_MEM
};

/* Message Structs */
struct wd_status_mesg 
{
	enum wd_mesg_type type;
	uint8_t battery;
	enum wd_ext_type ext_type;
};	

struct wd_btn_mesg 
{
	enum wd_mesg_type type;
	uint16_t buttons;
};

struct wd_acc_mesg 
{
	enum wd_mesg_type type;
	uint8_t acc[3];
};

struct wd_ir_src 
{
	char valid;
	uint16_t pos[2];
	int8_t size;
};

struct wd_ir_mesg 
{
	enum wd_mesg_type type;
	struct wd_ir_src src[WD_IR_SRC_COUNT];
};

struct wd_nunchuk_mesg 
{
	enum wd_mesg_type type;
	uint8_t stick[2];
	uint8_t acc[3];
	uint8_t buttons;
};

struct wd_classic_mesg 
{
	enum wd_mesg_type type;
	uint8_t l_stick[2];
	uint8_t r_stick[2];
	uint8_t l;
	uint8_t r;
	uint16_t buttons;
};

struct wd_balance_mesg 
{
	enum wd_mesg_type type;
	uint16_t right_top;
	uint16_t right_bottom;
	uint16_t left_top;
	uint16_t left_bottom;
};

struct wd_motionplus_mesg 
{
	enum wd_mesg_type type;
	uint16_t angle_rate[3];
	uint8_t low_speed[3];
};

struct wd_error_mesg 
{
	enum wd_mesg_type type;
	enum wd_error error;
};

struct write_seq 
{
	enum write_seq_type type;
	uint32_t report_offset;
	const void *data;
	uint16_t len;
	uint8_t flags;
};

/* State Structs */
struct nunchuk_state 
{
	uint8_t stick[2];
	uint8_t acc[3];
	uint8_t buttons;
};

struct classic_state 
{
	uint8_t l_stick[2];
	uint8_t r_stick[2];
	uint8_t l;
	uint8_t r;
	uint16_t buttons;
};

struct balance_state 
{
	uint16_t right_top;
	uint16_t right_bottom;
	uint16_t left_top;
	uint16_t left_bottom;
};

struct motionplus_state 
{
	uint16_t angle_rate[3];
	uint8_t low_speed[3];
};

union ext_state 
{
	struct nunchuk_state nunchuk;
	struct classic_state classic;
	struct balance_state balance;
	struct motionplus_state motionplus;
};

struct wd_state {
	uint8_t rpt_mode;
	uint8_t led;
	uint8_t rumble;
	uint8_t battery;
	uint16_t buttons;
	uint8_t acc[3];
	struct wd_ir_src ir_src[WD_IR_SRC_COUNT];
	enum wd_ext_type ext_type;
	union ext_state ext;
	enum wd_error error;
};

struct rw_mesg 
{
	enum rw_status type;
	uint8_t error;
	uint32_t offset;
	uint8_t len;
	char data[16];
};

union wd_mesg 
{
	enum wd_mesg_type type;
	struct wd_status_mesg status_mesg;
	struct wd_btn_mesg btn_mesg;
	struct wd_acc_mesg acc_mesg;
	struct wd_ir_mesg ir_mesg;
	struct wd_nunchuk_mesg nunchuk_mesg;
	struct wd_classic_mesg classic_mesg;
	struct wd_balance_mesg balance_mesg;
	struct wd_motionplus_mesg motionplus_mesg;
	struct wd_error_mesg error_mesg;
};

/* Typedefs */
typedef struct wiimote wiimote_t;
typedef void cwiid_mesg_callback_t(wiimote_t *, int, union wd_mesg [], struct timespec *);

/* Wiimote struct */
struct wiimote 
{
	int flags;
	int ctl_socket;
	int int_socket;
	pthread_t router_thread;
	pthread_t status_thread;
	pthread_t mesg_callback_thread;
	int mesg_pipe[2];
	int status_pipe[2];
	int rw_pipe[2];
	struct wd_state state;
	enum rw_status rw_status;
	cwiid_mesg_callback_t *mesg_callback;
	pthread_mutex_t state_mutex;
	pthread_mutex_t rw_mutex;
	pthread_mutex_t rpt_mutex;
	int id;
	const void *data;
};

/* Message arrays */
struct mesg_array 
{
	uint8_t count;
	struct timespec timestamp;
	union wd_mesg array[WD_MAX_MESG_COUNT];
};

struct balance_cal 
{
	uint16_t right_top[3];
	uint16_t right_bottom[3];
	uint16_t left_top[3];
	uint16_t left_bottom[3];
};
                                   
wiimote_t *wd_create_new_wii(int ctl_socket, int int_socket, int flags);
int wd_get_board_calibration_data(wiimote_t *wiimote, struct balance_cal *balance_cal);
int wd_read(wiimote_t *wiimote, uint8_t flags, uint32_t offset, uint16_t len, void *data);
int wd_send_rpt(wiimote_t *wiimote, uint8_t flags, uint8_t report, size_t len, const void *data);
int wd_verify_handshake(struct wiimote *wiimote);
int wd_full_read(int fd, void *buf, size_t len);
void *wd_router_thread(struct wiimote *wiimote);
void *wd_status_thread(struct wiimote *wiimote);
int wd_process_ext(struct wiimote *wiimote, unsigned char *data, unsigned char len, struct mesg_array *ma);
int wd_process_error(struct wiimote *wiimote, ssize_t len, struct mesg_array *ma);
int wd_cancel_rw(struct wiimote *wiimote);
int wd_write_mesg_array(struct wiimote *wiimote, struct mesg_array *ma);
int wd_write(wiimote_t *wiimote, uint8_t flags, uint32_t offset, uint16_t len, const void *data);
int wd_update_state(struct wiimote *wiimote, struct mesg_array *ma);
int wd_update_rpt_mode(struct wiimote *wiimote, int8_t rpt_mode);
int wd_process_read(struct wiimote *wiimote, unsigned char *data);
int wd_process_btn(struct wiimote *wiimote, const unsigned char *data, struct mesg_array *ma);
int wd_process_acc(struct wiimote *wiimote, const unsigned char *data, struct mesg_array *ma);
int wd_process_write(struct wiimote *wiimote, unsigned char *data);
int wd_process_status(struct wiimote *wiimote, const unsigned char *data, struct mesg_array *ma);
 
#endif