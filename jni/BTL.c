/*
 *
 *  Wii Balance Board Controller for Android
 *
 *  Copyright (C) 2011 Mohammad Hashemian (m.hashemian@gmail.com)
 *   
 *  The code in this module are partly borrowed from 
 *  www.pocketmagic.net (for BlueZ Bluetooth communication)
 *  and from CWiid project at www.abstrakraft.org/cwiid/
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
 *  All rights reserved.
 */ 

#include <string.h>
#include <jni.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include "bluetooth.h"
#include "l2cap.h"
#include "btutil.h"
#include "hci.h"
#include "android/log.h"
#include "wii_droid_defs.h"

#define GENERAL_ERROR				-1
#define NEGATIVE_DEVICE_COUNT		-2
#define WII_CONNECTION_CREATION_ERR -3
#define SOCKET_OPEN_FAILURE 		-4
#define NO_BT_DEV_FOUND				-5
#define NO_CONNECTION_CREATED		-6
#define BATTERY_LOW					-7
#define OPERATION_SUCCESSFUL 		1

/* Variable Definition */
struct wiimote *wiimote_obj = NULL;

int isCalibrationDataValid = FALSE;
struct balance_cal cal_data;

int isBalanceDataValid = FALSE;
struct wd_balance_mesg *balance_mesg;

int blnShouldRouterThreadContinue = 1;
int blnShouldStatusThreadContinue = 1;

int blnIsRouterThreadWorking = 0;
int blnIsStatusThreadWorking = 0;

int intBatteryLevel = 0;

JavaVM* jvm = 0;

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
	jvm = vm;
	//native lib loaded
	return JNI_VERSION_1_2; //1_2 1_4
}

void JNI_OnUnload(JavaVM *vm, void *reserved)
{
	jvm = 0;
	//__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Loaded revision 36");
	//native lib unloaded
}

/* Reads LIB version
*/
jstring Java_iEpi_Scale_BoardInterface_intReadVersion( JNIEnv* env,jobject thiz )
{
    return (*env)->NewStringUTF(env, 
				"Wii Balance Board Controller for Android. v1.0 (C)2011 Mohammad Hashemian, m.hashemian@gmail.com");
}

/* This function combines the operations of connection, getting the calibration data from the board,
	and start reading information.
	Returns:
		-1 if the Bluetooth device is not available, 
		0 if there is no device to connect to, 
		1 if the connection established successfully.		
*/
jint Java_iEpi_Scale_BoardInterface_ConnectCalibrateRead(JNIEnv* env, jobject thiz, int scantime)
{
	int intLoopCounter = 0;
	
	jint result = Java_iEpi_Scale_BoardInterface_intConnect(env, thiz, scantime);
	if(result != OPERATION_SUCCESSFUL)
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "StartupModule: Connection failed with error %d ...", result);
		return result;
	}

	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "StartupModule: Connection successful, continue ...");
	sleep(2);
	
	// Retrieve calibration data from the board ...
	int blnFinishedCalibration = FALSE;
	for(intLoopCounter = 0;intLoopCounter < MAX_CAL_TRIAL && !blnFinishedCalibration;intLoopCounter++)
	{
		result = Java_iEpi_Scale_BoardInterface_getCalibrationData(env, thiz);
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "StartupModule: Calibration data result is %d ...", result);
		if(result == OPERATION_SUCCESSFUL)
			if(Java_iEpi_Scale_BoardInterface_getIsCalibrationDataValid() == TRUE)
				blnFinishedCalibration = TRUE;
	}

	if(blnFinishedCalibration == FALSE)
	{
		Java_iEpi_Scale_BoardInterface_disconnect();
		return result;
	}
	
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "StartupModule: Making sure the battery level is enough ...");
	sleep(2);
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "StartupModule: Battery level was %.2X ...", intBatteryLevel);
	// Currently this section is deactivated, as when I tested the program on few boards, 
	// turned out that not all of them return the same value as battery level when they have a low battery. So for now, 
	// instead of checking this here, I check it in the Java code whether the value I get from sensors is minimum or not 
	// (-50 KG). If the board returns -50 or below, most probably there is an issue with the battery. 
	if(0)//intBatteryLevel < 0x00)
	{
		Java_iEpi_Scale_BoardInterface_disconnect();
		return BATTERY_LOW;
	}
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "StartupModule: Going to read data from the board ...");
	//
	// Start reading data from the board ...
	//
	int blnStartedReading = FALSE;
	for(intLoopCounter = 0;intLoopCounter < MAX_READ_TRIAL && !blnStartedReading;intLoopCounter++)
	{
		result = Java_iEpi_Scale_BoardInterface_startReadingData(env, thiz);
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "StartupModule: report result is %d ...", result);
		if(result == OPERATION_SUCCESSFUL)
			blnStartedReading = TRUE;
	}
	
	//
	// If starting the read process has failed, disconnect ...
	//
	if(blnStartedReading == FALSE)
	{
		Java_iEpi_Scale_BoardInterface_disconnect();
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "StartupModule: Reading failed. Returning error ...");
		return result;
	}
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "StartupModule: Done. Connection successful. ");
	return OPERATION_SUCCESSFUL;
}

/* Discover bluetooth devices and read Report Descriptor
	Returns: 
		WII_CONNECTION_CREATION_ERR		If connection to wii failed
		NO_CONNECTION_CREATED			If the discovery finishes, but no device is discovered to connect to.
		NEGATIVE_DEVICE_COUNT			If there is negative number of devices
		NO_BT_DEV_FOUND					If there is 0 device found
		SUCCESSFUL_DISCOVERY			The operation was successful
*/
jint Java_iEpi_Scale_BoardInterface_intConnect( JNIEnv* env,jobject thiz, int scantime )
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Discover: Entered the discovery function - Revision 39"); 
	//---
	bdaddr_t 	btDevAddr, dev;
	bdaddr_t 	src, dst, dst_first;
	uint8_t 	class[3];
	char 		strAddr[18];
	//-------------		
	bacpy(&btDevAddr, BDADDR_ANY);
	ba2str(&btDevAddr, strAddr);
	//
	// link to device
	//
	int devId = hci_devid(strAddr);
	if (devId < 0) 
	{
		devId = hci_get_route(NULL);
		hci_devba(devId, &src);
	}
	else
		bacpy(&src, &btDevAddr);
		
	//
	// open socket
	//
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Discover: trying to open the socket ...");
	int sock = hci_open_dev(devId);
	if (devId < 0 || sock < 0) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Failed to open the socket."); 
		return SOCKET_OPEN_FAILURE;
	}
		
	int length  = scantime;
	int flags   = IREQ_CACHE_FLUSH;
	int max_rsp = 255;
	inquiry_info *info;
	info = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));
	//
    // start searching for nearby devices
    //
	int num_rsp = hci_inquiry(devId, 
			length, //The inquiry lasts for at most 1.28 * length seconds
			max_rsp, //at most max_rsp devices will be returned
			NULL, 
			&info, 
			flags);
			
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Discover: Finished inquiry");
	if (num_rsp < 0)
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Negative number of devices discovered: %d",num_rsp);
		return NEGATIVE_DEVICE_COUNT;
	}
	else if(num_rsp == 0)
		return NO_BT_DEV_FOUND;

	int rsp_counter,j, err = -1, found = -1;
	for (rsp_counter = 0; rsp_counter < num_rsp; rsp_counter++) 
	{
		//
		// read remote name
		//
		char name[248] = {0};
		memset(name, 0, sizeof(name));
		if (hci_read_remote_name(sock, &(info+rsp_counter)->bdaddr, sizeof(name), name, 2000) < 0)
		        strcpy(name, "");
		//
		// read class
		//
		memcpy(class, (info+rsp_counter)->dev_class, 3);
		//
		// read Bluetooth remote address
		//
		bacpy(&dst, &(info+rsp_counter)->bdaddr);
		ba2str(&dst, strAddr);

		//
		// Compare the name of the recently found device with the Nintendo Balance Board name ...
		//
		if(//(strcmp(name,"Nintendo RVL-WBC-01") == 0) ||
		   (strcmp(strAddr,"00:26:59:2C:86:E8") == 0) ||
		   (strcmp(strAddr,"A4:C0:E1:93:D2:FC") == 0))
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Discover: Found a balance board ...");
			struct sockaddr_l2 remote_addr;
			int ctl_socket = -1, int_socket = -1; // Control and Interrupt socket.
		
			//
			// Connect to Wiimote 
			//
			//
			// Control Channel
			// 
			memset(&remote_addr, 0, sizeof remote_addr);
			remote_addr.l2_family = AF_BLUETOOTH;
			remote_addr.l2_bdaddr = (info+rsp_counter)->bdaddr;
			remote_addr.l2_psm = htobs(CTL_PSM);
			if ((ctl_socket = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) == -1) 
			{
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Discover: Socket creation error (control socket).");
				goto ERR_HND;
			}
			if (connect(ctl_socket, (struct sockaddr *)&remote_addr, sizeof remote_addr)) 
			{
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Discover: Cannot connect to control socket.");
				goto ERR_HND;
			}

			//
			// Interrupt Channel
			//
			remote_addr.l2_psm = htobs(INT_PSM);
			if ((int_socket = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) == -1) 
			{
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Discover: Error in creating interrupt socket.");
				goto ERR_HND;
			}
			if (connect(int_socket, (struct sockaddr *)&remote_addr, sizeof remote_addr)) 
			{
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Discover: Cannot connect to interrupt socket.");
				goto ERR_HND;
			}
		
			if ((wiimote_obj = wd_create_new_wii(ctl_socket, int_socket, flags)) == NULL) 
			{
				// Raises its own error 
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Discover: Error in creating a new Wii device.");
				goto ERR_HND;
			}
		
			return OPERATION_SUCCESSFUL;
		
		ERR_HND:
			// Close Sockets 
			if (ctl_socket != -1) 
			{
				if (close(ctl_socket))
				{
					__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Error in closing control socket.");
				}
			}
			if (int_socket != -1) 
			{
				if (close(int_socket)) 
				{
					__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Error in closing interrupt socket.");
				}
			}
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "intConnect: Returning WII_CONNECTION_CREATION_ERR ...");
			return WII_CONNECTION_CREATION_ERR;
		}
		else
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "found another device called %s\n", name);
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Device address is: %s\n", strAddr);
		}
	}
	if (sock >= 0) 
		close(sock);
	bt_free(info);
    //-------------
    return NO_CONNECTION_CREATED;
}

/* Requests the calibration data from the board and fills the related variables with the received 
   values. The relevant flag is set to true when the data is retrieved.
   Returns:
   	GENERAL_ERROR			If getting calibration data fails.
	OPERATION_SUCCESSFUL 	Otherwise   	
*/
jint Java_iEpi_Scale_BoardInterface_getCalibrationData( JNIEnv* env, jobject thiz)
{
	isCalibrationDataValid = FALSE;
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Discover: Getting balance board calibration data.");
	unsigned char buf[24];

	if (wd_read(wiimote_obj, WD_RW_REG, 0xa40024, 24, buf)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Read error (balancecal)");
		return GENERAL_ERROR;
	}
	
	(&cal_data)->right_top[0]    = ((uint16_t)buf[0]<<8 | (uint16_t)buf[1]);
	(&cal_data)->right_bottom[0] = ((uint16_t)buf[2]<<8 | (uint16_t)buf[3]);
	(&cal_data)->left_top[0]     = ((uint16_t)buf[4]<<8 | (uint16_t)buf[5]);
	(&cal_data)->left_bottom[0]  = ((uint16_t)buf[6]<<8 | (uint16_t)buf[7]);
	(&cal_data)->right_top[1]    = ((uint16_t)buf[8]<<8 | (uint16_t)buf[9]);
	(&cal_data)->right_bottom[1] = ((uint16_t)buf[10]<<8 | (uint16_t)buf[11]);
	(&cal_data)->left_top[1]     = ((uint16_t)buf[12]<<8 | (uint16_t)buf[13]);
	(&cal_data)->left_bottom[1]  = ((uint16_t)buf[14]<<8 | (uint16_t)buf[15]);
	(&cal_data)->right_top[2]    = ((uint16_t)buf[16]<<8 | (uint16_t)buf[17]);
	(&cal_data)->right_bottom[2] = ((uint16_t)buf[18]<<8 | (uint16_t)buf[19]);
	(&cal_data)->left_top[2]     = ((uint16_t)buf[20]<<8 | (uint16_t)buf[21]);
	(&cal_data)->left_bottom[2]  = ((uint16_t)buf[22]<<8 | (uint16_t)buf[23]);
	isCalibrationDataValid = TRUE;

	return OPERATION_SUCCESSFUL;
}

/* Sets the report mode of the board to continuously report the weight.
   Returns:
   	GENERAL_ERROR			If setting the report mode fails.
	OPERATION_SUCCESSFUL 	Otherwise
*/
jint Java_iEpi_Scale_BoardInterface_startReadingData( JNIEnv* env, jobject thiz)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Now is the time to set the report mode.");
	unsigned char report_mode = 0;
	toggle_bit(report_mode, WD_RPT_BALANCE);
	if (wd_update_rpt_mode(wiimote_obj, report_mode))
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Error setting report mode\n");
		return GENERAL_ERROR;
	}
	return OPERATION_SUCCESSFUL;
}

/* Stops the device from reporting the weight continuously.
*/ 
void Java_iEpi_Scale_BoardInterface_stopReadingData( JNIEnv* env, jobject thiz)
{
	// If the object for WiiMote is not created, return.
	if (!wiimote_obj)
		return;
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Now is the time to set the report mode.");
	unsigned char report_mode = 0;
	toggle_bit(report_mode, 0x00);
	if (wd_update_rpt_mode(wiimote_obj, report_mode))
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Error setting report mode\n");
	}
}

/* Disconnects from the board and releases the created resources.
*/
jint Java_iEpi_Scale_BoardInterface_disconnect()
{
	blnShouldRouterThreadContinue = 0;
	blnShouldStatusThreadContinue = 0;
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Disconnect called.");
	if (wiimote_obj) 
	{
		if (wiimote_obj->int_socket != -1) 
		{
			if (close(wiimote_obj->int_socket)) 
			{
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Error in closing interrupt socket.");
				//return GENERAL_ERROR;
			}
		}

		if (wiimote_obj->ctl_socket != -1) 
		{
			if (close(wiimote_obj->ctl_socket))
			{
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Error in closing control socket.");
				//return GENERAL_ERROR;
			}
		}
		
		int result1 = pthread_kill(wiimote_obj->router_thread,SIGUSR1);
		int result2 = pthread_kill(wiimote_obj->status_thread,SIGUSR1);
		
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Router thread is killed and the result is: %.2X", result1);
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Router thread is killed and the result is: %.2X", result2);
		
		/* Close threads *
		pthread_cancel(wiimote_obj->router_thread);
		if (pthread_join(wiimote_obj->router_thread, &pthread_ret)) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "THREAD JOIN ERROR (router thread)");
			return GENERAL_ERROR;
		}
		else if (!((pthread_ret == PTHREAD_CANCELED) && (pthread_ret == NULL))) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "BAD RETURN VALUE FROM ROUTER THREAD");
			return GENERAL_ERROR;
		}

		pthread_cancel(wiimote_obj->status_thread);
		if (pthread_join(wiimote_obj->status_thread, &pthread_ret)) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "THREAD JOIN ERROR (STATUS THREAD)");
			return GENERAL_ERROR;
		}
		else if (!((pthread_ret == PTHREAD_CANCELED) && (pthread_ret == NULL))) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "BAD RETURN VALUE FROM STATUS THREAD");
			return GENERAL_ERROR;
		}

		/* Close Pipes */
		if (close(wiimote_obj->mesg_pipe[0]) || close(wiimote_obj->mesg_pipe[1])) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "PIPE CLOSE ERROR (MESSAGE PIPE)");
			//return GENERAL_ERROR;
		}
		if (close(wiimote_obj->status_pipe[0]) || close(wiimote_obj->status_pipe[1])) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "PIPE CLOSE ERROR (STATUS PIPE)");
			//return GENERAL_ERROR;
		}
		if (close(wiimote_obj->rw_pipe[0]) || close(wiimote_obj->rw_pipe[1])) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "PIPE CLOSE ERROR (RW PIPE)");
			//return GENERAL_ERROR;
		}
		free(wiimote_obj);
	}

	return OPERATION_SUCCESSFUL;
}

/* Returns whether balance board weight values are valid or not
*/ 
jint Java_iEpi_Scale_BoardInterface_getIsBalanceDataValid()
{
	return isBalanceDataValid;
}

/* Returns the top right value of the board
*/ 
jint Java_iEpi_Scale_BoardInterface_getTopRightValue()
{
	return balance_mesg->right_top;
}

/* Returns the top left value of the board
*/ 
jint Java_iEpi_Scale_BoardInterface_getTopLeftValue()
{
	return balance_mesg->left_top;
}

/* Returns the bottom right value of the board
*/ 
jint Java_iEpi_Scale_BoardInterface_getBottomRightValue()
{
	return balance_mesg->right_bottom;
}

/* Returns the bottom left value of the board
*/ 
jint Java_iEpi_Scale_BoardInterface_getBottomLeftValue()
{
	return balance_mesg->left_bottom;
}

jint Java_iEpi_Scale_BoardInterface_getBatteryLevel()
{
	return intBatteryLevel;
}

/* Creates a new Wiimote object based on the connection information provided.
*/
wiimote_t *wd_create_new_wii(int ctl_socket, int int_socket, int flags)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Going to create a new wii device");
	struct	wiimote *new_wiimote = NULL;
	char	mesg_pipe_init = 0, 
			status_pipe_init = 0, 
			rw_pipe_init = 0,
			state_mutex_init = 0, 
			rw_mutex_init = 0, 
			rpt_mutex_init = 0,
			router_thread_init = 0, 
			status_thread_init = 0;
	void	*pthread_ret;

	/* Allocate wiimote */
	if ((new_wiimote = malloc(sizeof *new_wiimote)) == NULL) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Could not allocate enough memory for a wiimote object.");
		goto ERR_HND;
	}

	/* set sockets and flags */
	new_wiimote->ctl_socket = ctl_socket;
	new_wiimote->int_socket = int_socket;
	new_wiimote->flags = flags;

	new_wiimote->id = 1;

	/* Create pipes */
	if (pipe(new_wiimote->mesg_pipe)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Error in creating message pipe.");
		goto ERR_HND;
	}
	mesg_pipe_init = 1;
	if (pipe(new_wiimote->status_pipe)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Error in creating status pipe");
		goto ERR_HND;
	}
	status_pipe_init = 1;
	if (pipe(new_wiimote->rw_pipe)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Error in creating read/write pipe");
		goto ERR_HND;
	}
	rw_pipe_init = 1;

	/* Setup blocking */
	if (fcntl(new_wiimote->mesg_pipe[1], F_SETFL, O_NONBLOCK)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Error in setting the first message pipe as non-blocking.");
		goto ERR_HND;
	}
	if (new_wiimote->flags & WD_FLAG_NONBLOCK) 
	{
		if (fcntl(new_wiimote->mesg_pipe[0], F_SETFL, O_NONBLOCK)) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Error in setting the zeroth message pipe as non-blocking.");
			goto ERR_HND;
		}
	}

	/* Init mutexes */
	if (pthread_mutex_init(&new_wiimote->state_mutex, NULL)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Error in initialization of state mutex.");
		goto ERR_HND;
	}
	state_mutex_init = 1;
	if (pthread_mutex_init(&new_wiimote->rw_mutex, NULL)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Error in initialization of read/write mutex.");
		goto ERR_HND;
	}
	rw_mutex_init = 1;
	if (pthread_mutex_init(&new_wiimote->rpt_mutex, NULL)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Error in initialization of report mutex.");
		goto ERR_HND;
	}
	rpt_mutex_init = 1;

	/* Set rw_status before starting router thread */
	new_wiimote->rw_status = RW_IDLE;

	blnShouldRouterThreadContinue = 1;
	/* Launch interrupt socket listener and dispatch threads */
	if (pthread_create(&new_wiimote->router_thread, NULL, (void *(*)(void *))&wd_router_thread, new_wiimote)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Thread creation error (router thread)");
		goto ERR_HND;
	}
	router_thread_init = 1;

	blnShouldStatusThreadContinue = 1;	
	if (pthread_create(&new_wiimote->status_thread, NULL, (void *(*)(void *))&wd_status_thread, new_wiimote)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Thread creation error (status thread)");
		goto ERR_HND;
	}
	status_thread_init = 1;

	/* Success!  Update state */
	memset(&new_wiimote->state, 0, sizeof new_wiimote->state);
	new_wiimote->mesg_callback = NULL;
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_create_new_wii: Returning newly created mote.");
	return new_wiimote;

ERR_HND:
	/*
	TODO: This section needs to be edited in order to clean up the program after a fail.
	*/
	if (new_wiimote) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "Error in creating Wiimote device.");
		free(new_wiimote);
	}
	return NULL;
}

int wd_read(wiimote_t *wiimote, uint8_t flags, uint32_t offset, uint16_t len, void *data)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_read: First reading some data.");
	unsigned char buf[RPT_READ_REQ_LEN];
	struct rw_mesg mesg;
	unsigned char *cursor;
	int ret = 0;

	/* Compose read request packet */
	buf[0]=flags & (WD_RW_EEPROM | WD_RW_REG);
	buf[1]=(unsigned char)((offset>>16) & 0xFF);
	buf[2]=(unsigned char)((offset>>8)  & 0xFF);
	buf[3]=(unsigned char)( offset      & 0xFF);
	buf[4]=(unsigned char)((len>>8)     & 0xFF);
	buf[5]=(unsigned char)( len         & 0xFF);

	/* Setup read info */
	//__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_read: setup read info.");
	wiimote->rw_status = RW_READ;

	/* TODO: Document: user is responsible for ensuring that read/write
	 * operations are not in flight while disconnecting.  Nothing serious,
	 * just accesses to freed memory */
	/* Send read request packet */
	//__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_read: Send read request packet.");
	if (wd_send_rpt(wiimote, 0, RPT_READ_REQ, RPT_READ_REQ_LEN, buf)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_read: Report send error (read)");
		ret = -1;
		goto CODA;
	}

	/* TODO:Better sanity checks (offset) */
	/* Read packets */
	//__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_read: Going to read packets ....");
	for (cursor = data; cursor - (unsigned char *)data < len;cursor += mesg.len) 
	{
		if (wd_full_read(wiimote->rw_pipe[0], &mesg, sizeof mesg)) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_read: Pipe read error (rw pipe)");
			ret = -1;
			goto CODA;
		}

		if (mesg.type == RW_CANCEL) 
		{
			ret = -1;
			goto CODA;
		}
		else if (mesg.type != RW_READ) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_read: Unexpected write message");
			ret = -1;
			goto CODA;
		}

		if (mesg.error) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_read: Wiimote read error");
			ret = -1;
			goto CODA;
		}
		//__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_read: No error, only one memory copy has left.");
		memcpy(cursor, &mesg.data, mesg.len);
	}

CODA:
	/* Clear rw_status */
	wiimote->rw_status = RW_IDLE;
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_read: Done. Return.");
	return ret;
}

int wd_send_rpt(wiimote_t *wiimote, uint8_t flags, uint8_t report, size_t len, const void *data)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_send_rpt: Going to ask for read info.");
	unsigned char *buf;

	if ((buf = malloc((len*2) * sizeof *buf)) == NULL) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_send_rpt: Memory allocation error (mesg array)");
		return -1;
	}

	buf[0] = BT_TRANS_SET_REPORT | BT_PARAM_OUTPUT;
	buf[1] = report;
	memcpy(buf+2, data, len);
	if (!(flags & WD_SEND_RPT_NO_RUMBLE)) 
	{
		buf[2] |= wiimote->state.rumble;
	}

	if (write(wiimote->ctl_socket, buf, len+2) != (ssize_t)(len+2)) 
	{
		free(buf);
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_send_rpt: error in calling write");
		return -1;
	}
	else if (wd_verify_handshake(wiimote)) 
	{
		free(buf);
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_send_rpt: error in calling verify handshake");
		return -1;
	}
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_send_rpt: Done requesting. Going back.");
	return 0;
}

int wd_verify_handshake(struct wiimote *wiimote)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_verify_handshake: Starting to handshake.");
	unsigned char handshake;
	if (read(wiimote->ctl_socket, &handshake, 1) != 1) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_verify_handshake: Socket read error (handshake)");
		return -1;
	}
	else if ((handshake & BT_TRANS_MASK) != BT_TRANS_HANDSHAKE) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_verify_handshake: Handshake expected, non-handshake received");
		return -1;
	}
	else if ((handshake & BT_PARAM_MASK) != BT_PARAM_SUCCESSFUL) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_verify_handshake: Non-successful handshake");
		return -1;
	}
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_verify_handshake: Done handshaking, going back.");
	return 0;
}

int wd_full_read(int fd, void *buf, size_t len)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_full_read: Full read is called.");
	ssize_t last_len = 0;

	do 
	{
		//__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_full_read: Started the do loop.");
		if ((last_len = read(fd, buf, len)) == -1) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_full_read: Read failed!");
			return -1;
		}
		//__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_full_read: Read done. len = %d, buf = %d",len, buf);
		len -= last_len;
		buf += last_len;
	} while (len > 0);
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_full_read: Done full read, going back.");
	return 0;
}

void *wd_router_thread(struct wiimote *wiimote)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_router_thread: Started wd_router_thread");
	unsigned char buf[READ_BUF_LEN];
	ssize_t len;
	struct mesg_array ma;
	char err, print_clock_err = 1;
	
	JNIEnv* env = 0;
	(*jvm)->AttachCurrentThread(jvm,&env, NULL);
	blnIsRouterThreadWorking = 1;
	
	while (blnShouldRouterThreadContinue) 
	{
		/* Read packet */
		len = read(wiimote->int_socket, buf, READ_BUF_LEN);
		ma.count = 0;
		if (clock_gettime(CLOCK_REALTIME, &ma.timestamp)) 
		{
			if (print_clock_err) 
			{
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_router_thread: clock_gettime error");
				print_clock_err = 0;
			}
		}
		err = 0;
		if ((len == -1) || (len == 0)) 
		{
			wd_process_error(wiimote, len, &ma);
			wd_write_mesg_array(wiimote, &ma);
			/* Quit! */
			break;
		}
		else 
		{
			/* Verify first byte (DATA/INPUT) which should be 0xA1, refer to the wiki for more info. */
			if (buf[0] != (BT_TRANS_DATA | BT_PARAM_INPUT)) 
			{
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_router_thread: Invalid packet type");
			}

			/* Main switch */
			if(buf[1] != 50)
			{
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"%.2X %.2X %.2X %.2X  %.2X %.2X %.2X %.2X\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"%.2X %.2X %.2X %.2X  %.2X %.2X %.2X %.2X\n", buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"%.2X %.2X %.2X %.2X  %.2X %.2X %.2X %.2X\n", buf[16], buf[17], buf[18], buf[19], buf[20], buf[21], buf[22], buf[23]);
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"\n");//*/
			}
			// Extract some required information from received packets ... 
			if(buf[1] == 32)
			{
				// Turned out that if the battery level is low, the report mode only returns one set of 
				// results of type 0x32 (refer to WiiBrew WiiMote for more information on the packet)
				// instead of continues 0x32 packets. In this case, all EE bytes in the returned packet 
				// is set to zero. Therefore the calculated weight is not correct. The battery level is 
				// received in packet type 0x20 (status report) at the 8th byte. Here I store the value 
				// of the battery level, so the system can use it later.
				intBatteryLevel = buf[7];
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG, "wd_router_thread: Battery level was %.2X ...", intBatteryLevel);
			}
			// Check the message type and act accordingly ...
			switch (buf[1]) 
			{
			case RPT_STATUS: // 0x20
				//__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RPT_STATUS");
				err = wd_process_status(wiimote, &buf[2], &ma);
				break;
			case RPT_READ_DATA: // 0x21
				//__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RPT_READ_DATA");
				err = wd_process_read(wiimote, &buf[4]) ||
				      wd_process_btn(wiimote, &buf[2], &ma);
				break;
			case RPT_WRITE_ACK: // 0x22
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RPT_WRITE_ACK");
				err = wd_process_write(wiimote, &buf[2]);
				break;
			case RPT_BTN: // 0x30
				//__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RPT_BTN");
				err = wd_process_btn(wiimote, &buf[2], &ma);
				break;
			case RPT_BTN_ACC: // 0x31
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RPT_BTN_ACC");
				err = wd_process_btn(wiimote, &buf[2], &ma) ||
				      wd_process_acc(wiimote, &buf[4], &ma);
				break;
			case RPT_BTN_EXT8: // 0x32
				err = wd_process_ext(wiimote, &buf[4], 8, &ma);
				break;
			case RPT_BTN_ACC_IR12: // 0x33
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RPT_BTN_ACC_IR12");
				break;
			case RPT_BTN_EXT19: // 0x34
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RPT_BTN_EXT19");
				err = wd_process_ext(wiimote, &buf[4], 19, &ma);
				break;
			case RPT_BTN_ACC_EXT16: // 0x35
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RPT_BTN_EXT16");
				err = wd_process_ext(wiimote, &buf[7], 16, &ma);
				break;
			case RPT_BTN_IR10_EXT9: // 0x36
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RPT_BTN_IR10_EXT9");
				err = wd_process_ext(wiimote, &buf[14], 9, &ma);
				break;
			case RPT_BTN_ACC_IR10_EXT6: // 0x37
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RPT_BTN_ACC_IR10_EXT6");
				err = wd_process_ext(wiimote, &buf[17], 6, &ma);
				break;
			case RPT_EXT21: // 0x3D
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RPT_EXT21");
				err = wd_process_ext(wiimote, &buf[2], 21, &ma);
				break;
			case RPT_BTN_ACC_IR36_1: // 0x3E
			case RPT_BTN_ACC_IR36_2: // 0x3F
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Unsupported report type received (interleaved data)");
				err = 1;
				break;
			default:
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Unknown message type. The message is: %d",buf[1]);
				err = 1;
				break;
			}

			if (!err && (ma.count > 0)) 
			{
				if (wd_update_state(wiimote, &ma)) 
				{
					__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"State update error");
				}
				if (wiimote->flags & WD_FLAG_MESG_IFC) 
				{
					/* prints its own errors */
					//wd_write_mesg_array(wiimote, &ma);
				}
			}
		}
	}
	
	(*jvm)->DetachCurrentThread(jvm);
	env = 0;
	blnIsRouterThreadWorking = 0;
	
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"finished wd_router_thread");
	return NULL;
}

void *wd_status_thread(struct wiimote *wiimote)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Started wd_status_thread");
	
	struct mesg_array ma;
	struct wd_status_mesg *status_mesg;
	unsigned char buf[2];

	ma.count = 1;
	status_mesg = &ma.array[0].status_mesg;
	
	JNIEnv* env = 0;
	(*jvm)->AttachCurrentThread(jvm,&env, NULL);
	
	blnIsStatusThreadWorking = 1;

	while (blnShouldStatusThreadContinue) 
	{
		if (wd_full_read(wiimote->status_pipe[0], status_mesg, sizeof *status_mesg)) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Pipe read error (status)");
			/* Quit! */
			break;
		}

		if (status_mesg->type != WD_MESG_STATUS) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Bad message on status pipe");
			continue;
		}

		if (status_mesg->ext_type == WD_EXT_UNKNOWN) 
		{
			/* Read extension ID */
			if (wd_read(wiimote, WD_RW_REG, 0xA400FE, 2, &buf)) 
			{
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Read error (extension error)");
				status_mesg->ext_type = WD_EXT_UNKNOWN;
			}
			/* If the extension didn't change, or if the extension is a
			 * MotionPlus, no init necessary */
			switch ((buf[0] << 8) | buf[1]) 
			{
			case EXT_NONE:
				status_mesg->ext_type = WD_EXT_NONE;
				break;
			case EXT_NUNCHUK:
				status_mesg->ext_type = WD_EXT_NUNCHUK;
				break;
			case EXT_CLASSIC:
				status_mesg->ext_type = WD_EXT_CLASSIC;
				break;
			case EXT_BALANCE:
				status_mesg->ext_type = WD_EXT_BALANCE;
				break;
			case EXT_MOTIONPLUS:
				status_mesg->ext_type = WD_EXT_MOTIONPLUS;
				break;
			case EXT_PARTIAL:
				/* Everything (but MotionPlus) shows up as partial until initialized */
				buf[0] = 0x55;
				buf[1] = 0x00;
				/* Initialize extension register space */
				if (wd_write(wiimote, WD_RW_REG, 0xA400F0, 1, &buf[0])) 
				{
					__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Extension initialization error");
					status_mesg->ext_type = WD_EXT_UNKNOWN;
				}
				else if (wd_write(wiimote, WD_RW_REG, 0xA400FB, 1, &buf[1])) 
				{
						__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Extension initialization error");
						status_mesg->ext_type = WD_EXT_UNKNOWN;
				}
				/* Read extension ID */
				else if (wd_read(wiimote, WD_RW_REG, 0xA400FE, 2, &buf)) 
				{
					__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Read error (extension error)");
					status_mesg->ext_type = WD_EXT_UNKNOWN;
				}
				else 
				{
					switch ((buf[0] << 8) | buf[1]) 
					{
					case EXT_NONE:
					case EXT_PARTIAL:
						status_mesg->ext_type = WD_EXT_NONE;
						break;
					case EXT_NUNCHUK:
						status_mesg->ext_type = WD_EXT_NUNCHUK;
						break;
					case EXT_CLASSIC:
						status_mesg->ext_type = WD_EXT_CLASSIC;
						break;
					case EXT_BALANCE:
						status_mesg->ext_type = WD_EXT_BALANCE;
						break;
					default:
						status_mesg->ext_type = WD_EXT_UNKNOWN;
						break;
					}
				}
				break;
			}
		}

		if (wd_update_state(wiimote, &ma)) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"State update error");
		}
		if (wd_update_rpt_mode(wiimote, -1)) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Error reseting report mode");
		}
		if ((wiimote->state.rpt_mode & WD_RPT_STATUS) &&
		  (wiimote->flags & WD_FLAG_MESG_IFC)) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Condition 3");
			if (wd_write_mesg_array(wiimote, &ma)) 
			{
				/* prints its own errors */
			}
		}
	}
	
	(*jvm)->DetachCurrentThread(jvm);
	env = 0;
	blnIsStatusThreadWorking = 0;
	
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Finished wd_status_thread");
	return NULL;
}

int wd_process_ext(struct wiimote *wiimote, unsigned char *data, unsigned char len, struct mesg_array *ma)
{
	isBalanceDataValid = FALSE;
//	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_process_ext: Set the balance data as invalid. Going to get a new set.");
	int i;

	switch (wiimote->state.ext_type) 
	{
	case WD_EXT_NONE:
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"There is no extension! can you believe it?");
		break;
	case WD_EXT_UNKNOWN:
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Received unknown extension report");
		break;
	case WD_EXT_NUNCHUK:
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Received nunchuk extension report");
		break;
	case WD_EXT_CLASSIC:
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Received classic extension report");
		break;
	case WD_EXT_BALANCE:
		if (wiimote->state.rpt_mode & WD_RPT_BALANCE) 
		{
			balance_mesg = &ma->array[ma->count++].balance_mesg;
			balance_mesg->type = WD_MESG_BALANCE;
			balance_mesg->right_top = ((uint16_t)data[0]<<8 | (uint16_t)data[1]);
			balance_mesg->right_bottom = ((uint16_t)data[2]<<8 | (uint16_t)data[3]);
			balance_mesg->left_top = ((uint16_t)data[4]<<8 | (uint16_t)data[5]);
			balance_mesg->left_bottom = ((uint16_t)data[6]<<8 | (uint16_t)data[7]);
//			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Updated the weight again! RT: %d, RB: %d, LT: %d, LB: %d, COUNT: %d", 
//					balance_mesg->right_top, 
//					balance_mesg->right_bottom, 
//					balance_mesg->left_top, 
//					balance_mesg->left_bottom,
//					ma->count);
			isBalanceDataValid = TRUE;
		}
		break;
	case WD_EXT_MOTIONPLUS:
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Received motionplus extension report");
		break;
	}
//	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_process_ext: Done.");
	return 0;
}

int wd_process_error(struct wiimote *wiimote, ssize_t len, struct mesg_array *ma)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_process_error: Started wd_process_error");
	struct wd_error_mesg *error_mesg;

	error_mesg = &ma->array[ma->count++].error_mesg;
	error_mesg->type = WD_MESG_ERROR;
	if (len == 0) 
	{
		error_mesg->error = WD_ERROR_DISCONNECT;
	}
	else 
	{
		error_mesg->error = WD_ERROR_COMM;
	}

	if (wd_cancel_rw(wiimote)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_process_error: RW cancel error");
	}

	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_process_error: Finished wd_process_error");
	return 0;
}

int wd_cancel_rw(struct wiimote *wiimote)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"cancel_rw: Started wd_cancel_rw");
	struct rw_mesg rw_mesg;

	rw_mesg.type = RW_CANCEL;

	if (write(wiimote->rw_pipe[1], &rw_mesg, sizeof rw_mesg) != sizeof rw_mesg) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"cancel_rw: Pipe write error (rw)");
		return -1;
	}
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"cancel_rw: Finished cancel_rw");
	return 0;
}

int wd_write_mesg_array(struct wiimote *wiimote, struct mesg_array *ma)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Called");
	ssize_t len = (void *)&ma->array[ma->count] - (void *)ma;
	int ret = 0;

	/* This must remain a single write operation to ensure atomicity,
	 * which is required to avoid mutexes and cancellation issues */
	if (write(wiimote->mesg_pipe[1], ma, len) != len) 
	{
		if (errno == EAGAIN) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Mesg pipe overflow");
			if (fcntl(wiimote->mesg_pipe[1], F_SETFL, 0)) 
			{
				__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"File control error (mesg pipe)");
				ret = -1;
			}
			else 
			{
				if (write(wiimote->mesg_pipe[1], ma, len) != len) 
				{
					__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Pipe write error (mesg pipe)");
					ret = -1;
				}
				if (fcntl(wiimote->mesg_pipe[1], F_SETFL, O_NONBLOCK)) 
				{
					__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"File control error (mesg pipe");
				}
			}
		}
		else 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Pipe write error (mesg pipe)");
			ret = -1;
		}
	}
	return ret;
}

int wd_write(wiimote_t *wiimote, uint8_t flags, uint32_t offset, uint16_t len, const void *data)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Started wd_write");
	unsigned char buf[RPT_WRITE_LEN];
	uint16_t sent=0;
	struct rw_mesg mesg;
	int ret = 0;

	/* Compose write packet header */
	buf[0]=flags;

	/* Lock wiimote rw access */
	if (pthread_mutex_lock(&wiimote->rw_mutex)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Mutex lock error (rw mutex)");
		return -1;
	}

	/* Send packets */
	wiimote->rw_status = RW_WRITE;
	while (sent<len) 
	{
		/* Compose write packet */
		buf[1]=(unsigned char)(((offset+sent)>>16) & 0xFF);
		buf[2]=(unsigned char)(((offset+sent)>>8) & 0xFF);
		buf[3]=(unsigned char)((offset+sent) & 0xFF);
		if (len-sent >= 0x10) 
		{
			buf[4]=(unsigned char)0x10;
		}
		else 
		{
			buf[4]=(unsigned char)(len-sent);
		}
		memcpy(buf+5, data+sent, buf[4]);

		if (wd_send_rpt(wiimote, 0, RPT_WRITE, RPT_WRITE_LEN, buf)) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Report send error (write)");
			ret = -1;
			goto CODA;
		}

		/* Read packets from pipe */
		if (read(wiimote->rw_pipe[0], &mesg, sizeof mesg) != sizeof mesg) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Pipe read error (rw pipe)");
			ret = -1;
			goto CODA;
		}

		if (mesg.type == RW_CANCEL) 
		{
			ret = -1;
			goto CODA;
		}
		else if (mesg.type != RW_WRITE) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Unexpected read message");
			ret = -1;
			goto CODA;
		}

		if (mesg.error) 
		{
			__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Wiimote write error");
			ret = -1;
			goto CODA;
		};

		sent+=buf[4];
	}
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Finished wd_write successfully");
CODA:
	/* Clear rw_status */
	wiimote->rw_status = RW_IDLE;

	/* Unlock rw_mutex */
	if (pthread_mutex_unlock(&wiimote->rw_mutex)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Mutex unlock error (rw_mutex) - deadlock warning");
	}
	
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Finished wd_write");
	return ret;
}

int wd_update_state(struct wiimote *wiimote, struct mesg_array *ma)
{
	int i;
	union wd_mesg *mesg;

	if (pthread_mutex_lock(&wiimote->state_mutex)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Mutex lock error (state mutex)");
		return -1;
	}

	for (i=0; i < ma->count; i++) 
	{
		mesg = &ma->array[i];

		switch (mesg->type) 
		{
		case WD_MESG_STATUS:
			wiimote->state.battery = mesg->status_mesg.battery;
			if (wiimote->state.ext_type != mesg->status_mesg.ext_type) 
			{
				memset(&wiimote->state.ext, 0, sizeof wiimote->state.ext);
				wiimote->state.ext_type = mesg->status_mesg.ext_type;
			}
			break;
		case WD_MESG_BTN:
			wiimote->state.buttons = mesg->btn_mesg.buttons;
			break;
		case WD_MESG_ACC:
			memcpy(wiimote->state.acc, mesg->acc_mesg.acc,sizeof wiimote->state.acc);
			break;
		case WD_MESG_IR:
			memcpy(wiimote->state.ir_src, mesg->ir_mesg.src,sizeof wiimote->state.ir_src);
			break;
		case WD_MESG_NUNCHUK:
			memcpy(wiimote->state.ext.nunchuk.stick,
			       mesg->nunchuk_mesg.stick,
			       sizeof wiimote->state.ext.nunchuk.stick);
			memcpy(wiimote->state.ext.nunchuk.acc,
			       mesg->nunchuk_mesg.acc,
			       sizeof wiimote->state.ext.nunchuk.acc);
			wiimote->state.ext.nunchuk.buttons = mesg->nunchuk_mesg.buttons;
			break;
		case WD_MESG_CLASSIC:
			memcpy(wiimote->state.ext.classic.l_stick,
			       mesg->classic_mesg.l_stick,
			       sizeof wiimote->state.ext.classic.l_stick);
			memcpy(wiimote->state.ext.classic.r_stick,
			       mesg->classic_mesg.r_stick,
			       sizeof wiimote->state.ext.classic.r_stick);
			wiimote->state.ext.classic.l = mesg->classic_mesg.l;
			wiimote->state.ext.classic.r = mesg->classic_mesg.r;
			wiimote->state.ext.classic.buttons = mesg->classic_mesg.buttons;
			break;
		case WD_MESG_BALANCE:
			wiimote->state.ext.balance.right_top = mesg->balance_mesg.right_top;
			wiimote->state.ext.balance.right_bottom = mesg->balance_mesg.right_bottom;
			wiimote->state.ext.balance.left_top = mesg->balance_mesg.left_top;
			wiimote->state.ext.balance.left_bottom = mesg->balance_mesg.left_bottom;
			break;
		case WD_MESG_MOTIONPLUS:
			memcpy(wiimote->state.ext.motionplus.angle_rate,
			       mesg->motionplus_mesg.angle_rate,
			       sizeof wiimote->state.ext.motionplus.angle_rate);
			memcpy(wiimote->state.ext.motionplus.low_speed,
			       mesg->motionplus_mesg.low_speed,
			       sizeof wiimote->state.ext.motionplus.low_speed);
			break;
		case WD_MESG_ERROR:
			wiimote->state.error = mesg->error_mesg.error;
			break;
		case WD_MESG_UNKNOWN:
			/* do nothing, error has already been printed */
			break;
		}
	}

	if (pthread_mutex_unlock(&wiimote->state_mutex)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Mutex unlock error (state mutex) - deadlock warning");
		return -1;
	}
	return 0;
}

int wd_update_rpt_mode(struct wiimote *wiimote, int8_t rpt_mode)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Started wd_update_rpt_mode");
	unsigned char buf[RPT_MODE_BUF_LEN];
	uint8_t rpt_type;
	struct write_seq *ir_enable_seq;
	int seq_len;

	/* rpt_mode = bitmask of requested report types */
	/* rpt_type = report id sent to the wiimote */
	if (pthread_mutex_lock(&wiimote->rpt_mutex)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Mutex lock error (rpt mutex)");
		return -1;
	}

	/* -1 updates the reporting mode using old rpt_mode
	 * (reporting type may change if extensions are
	 * plugged in/unplugged */
	if (rpt_mode == -1) 
	{
		rpt_mode = wiimote->state.rpt_mode;
	}

	/* Pick a report mode based on report flags *
	if ((rpt_mode & WD_RPT_EXT) &&
	    ((wiimote->state.ext_type == WD_EXT_NUNCHUK) ||
	    (wiimote->state.ext_type == WD_EXT_CLASSIC) ||
	    (wiimote->state.ext_type == WD_EXT_MOTIONPLUS))) 
	{
		if (rpt_mode & WD_RPT_ACC) 
		{
			rpt_type = RPT_BTN_ACC_EXT16;
		}
		else if (rpt_mode & WD_RPT_BTN) 
		{
			rpt_type = RPT_BTN_EXT8;
		}
		else 
		{
			rpt_type = RPT_EXT21;
		}	
	}
	else if ((rpt_mode & WD_RPT_EXT) && wiimote->state.ext_type == WD_EXT_BALANCE) 
	{*/
	rpt_type = RPT_BTN_EXT8;
	//}

	/* Send SET_REPORT */
	buf[0] = (wiimote->flags & WD_FLAG_CONTINUOUS) ? 0x04 : 0;
	buf[1] = rpt_type;
	if (wd_send_rpt(wiimote, 0, RPT_RPT_MODE, RPT_MODE_BUF_LEN, buf)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Send report error (report mode)");
		return -1;
	}

	/* clear state for unreported data */
	if (WD_RPT_BTN & ~rpt_mode & wiimote->state.rpt_mode) 
	{
		wiimote->state.buttons = 0;
	}
	if (WD_RPT_ACC & ~rpt_mode & wiimote->state.rpt_mode) 
	{
		memset(wiimote->state.acc, 0, sizeof wiimote->state.acc);
	}
	if (WD_RPT_IR & ~rpt_mode & wiimote->state.rpt_mode) 
	{
		memset(wiimote->state.ir_src, 0, sizeof wiimote->state.ir_src);
	}
	if ((wiimote->state.ext_type == WD_EXT_NUNCHUK) &&
	    (WD_RPT_NUNCHUK & ~rpt_mode & wiimote->state.rpt_mode)) 
	{
		memset(&wiimote->state.ext, 0, sizeof wiimote->state.ext);
	}
	else if ((wiimote->state.ext_type == WD_EXT_CLASSIC) &&
			 (WD_RPT_CLASSIC & ~rpt_mode & wiimote->state.rpt_mode)) 
	{
		memset(&wiimote->state.ext, 0, sizeof wiimote->state.ext);
	}
	else if ((wiimote->state.ext_type == WD_EXT_BALANCE) &&
			 (WD_RPT_BALANCE & ~rpt_mode & wiimote->state.rpt_mode)) 
	{
		memset(&wiimote->state.ext, 0, sizeof wiimote->state.ext);
	}
	else if ((wiimote->state.ext_type == WD_EXT_MOTIONPLUS) &&
	  (WD_RPT_MOTIONPLUS & ~rpt_mode & wiimote->state.rpt_mode)) 
	{
		memset(&wiimote->state.ext, 0, sizeof wiimote->state.ext);
	}

	wiimote->state.rpt_mode = rpt_mode;

	if (pthread_mutex_unlock(&wiimote->rpt_mutex)) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Mutex unlock error (rpt mutex) - deadlock warning");
		return -1;
	}
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Finished wd_update_rpt_mode");
	return 0;
}

int wd_process_read(struct wiimote *wiimote, unsigned char *data)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Started wd_process_read");
	struct rw_mesg rw_mesg;

	if (wiimote->rw_status != RW_READ) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Received unexpected read report");
		return -1;
	}

	rw_mesg.type = RW_READ;
	rw_mesg.len = (data[0]>>4)+1;
	rw_mesg.error = data[0] & 0x0F;
	memcpy(&rw_mesg.data, data+3, rw_mesg.len);

	if (write(wiimote->rw_pipe[1], &rw_mesg, sizeof rw_mesg) != sizeof rw_mesg) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RW pipe write error");
		return -1;
	}

	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Finished wd_process_read");
	return 0;
}

int wd_process_btn(struct wiimote *wiimote, const unsigned char *data, struct mesg_array *ma)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Started wd_process_btn");
	struct wd_btn_mesg *btn_mesg;
	uint16_t buttons;

	buttons = (data[0] & BTN_MASK_0)<<8 |
	          (data[1] & BTN_MASK_1);
	if (wiimote->state.rpt_mode & WD_RPT_BTN) 
	{
		if ((wiimote->state.buttons != buttons) ||
		  (wiimote->flags & WD_FLAG_REPEAT_BTN)) 
		{
			btn_mesg = &ma->array[ma->count++].btn_mesg;
			btn_mesg->type = WD_MESG_BTN;
			btn_mesg->buttons = buttons;
		}
	}

	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Finished wd_process_btn");
	return 0;
}

int wd_process_acc(struct wiimote *wiimote, const unsigned char *data, struct mesg_array *ma)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Started wd_process_acc");
	struct wd_acc_mesg *acc_mesg;

	if (wiimote->state.rpt_mode & WD_RPT_ACC) 
	{
		acc_mesg = &ma->array[ma->count++].acc_mesg;
		acc_mesg->type = WD_MESG_ACC;
		acc_mesg->acc[WD_X] = data[0];
		acc_mesg->acc[WD_Y] = data[1];
		acc_mesg->acc[WD_Z] = data[2];
	}
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Finished wd_process_acc");
	return 0;
}

int wd_process_write(struct wiimote *wiimote, unsigned char *data)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Started wd_process_write");
	struct rw_mesg rw_mesg;

	if (wiimote->rw_status != RW_WRITE) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Received unexpected write report");
		return -1;
	}

	rw_mesg.type = RW_WRITE;
	rw_mesg.error = data[0];

	if (write(wiimote->rw_pipe[1], &rw_mesg, sizeof rw_mesg) != sizeof rw_mesg) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"RW pipe write error");
		return -1;
	}

	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Finished wd_process_write");
	return 0;
}

int wd_process_status(struct wiimote *wiimote, const unsigned char *data, struct mesg_array *ma)
{
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"wd_process_status: Started process_status");
	struct wd_status_mesg status_mesg;

	status_mesg.type = WD_MESG_STATUS;
	status_mesg.battery = data[5];
	if (data[2] & 0x02) 
	{
		/* wd_status_thread will figure out what it is */
		status_mesg.ext_type = WD_EXT_UNKNOWN;
	}
	else 
	{
		status_mesg.ext_type = WD_EXT_NONE;
	}

	if (write(wiimote->status_pipe[1], &status_mesg, sizeof status_mesg) != sizeof status_mesg) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Status pipe write error");
		return -1;
	}
	__android_log_print(ANDROID_LOG_DEBUG, DEBUG_TAG,"Finished process_status");
	return 0;
}

/*jint Java_iEpi_Scale_BoardInterface_intSetMode( JNIEnv* env,jobject thiz,int mode)
{
	bdaddr_t 	btDevAddr;
	bdaddr_t 	src, dst, dst_first;	
	int 		err, dd;
	char 		strAddr[18];

	bacpy(&btDevAddr, BDADDR_ANY);
	ba2str(&btDevAddr, strAddr);

	//
	// Trying to get connected to the Bluetooth Adapter.
	//
	int dev_id = hci_devid(strAddr);
	if (dev_id < 0) 
	{
		dev_id = hci_get_route(NULL);
		hci_devba(dev_id, &src);
	} 
	else
	{
		bacpy(&src, &btDevAddr);
	}

	dd = hci_open_dev(dev_id);
	if (dev_id < 0 || dd < 0) 
	{
	        return -1;
	}

	hci_close_dev(dd);
	return err;
}
*/

/* Returns whether balance board calibration values are valid or not
*/ 
jint Java_iEpi_Scale_BoardInterface_getIsCalibrationDataValid()
{
	return isCalibrationDataValid;
}

/* Returns the 0st value for right top of calibration data
*/ 
jint Java_iEpi_Scale_BoardInterface_getCalRightTop0()
{
	return (&cal_data)->right_top[0];
}

/* Returns the 1st value for right top of calibration data
*/ 
jint Java_iEpi_Scale_BoardInterface_getCalRightTop1()
{
	return (&cal_data)->right_top[1];
}

/* Returns the 2st value for right top of calibration data
*/ 
jint Java_iEpi_Scale_BoardInterface_getCalRightTop2()
{
	return (&cal_data)->right_top[2];
}

/* Returns the 0st value for left top of calibration data
*/ 
jint Java_iEpi_Scale_BoardInterface_getCalLeftTop0()
{
	return (&cal_data)->left_top[0];
}

/* Returns the 1st value for left top of calibration data
*/ 
jint Java_iEpi_Scale_BoardInterface_getCalLeftTop1()
{
	return (&cal_data)->left_top[1];
}

/* Returns the 2st value for right top of calibration data
*/ 
jint Java_iEpi_Scale_BoardInterface_getCalLeftTop2()
{
	return (&cal_data)->left_top[2];
}

/* Returns the 0st value for right bottom of calibration data
*/ 
jint Java_iEpi_Scale_BoardInterface_getCalRightBottom0()
{
	return (&cal_data)->right_bottom[0];
}

/* Returns the 1st value for right bottom of calibration data
*/ 
jint Java_iEpi_Scale_BoardInterface_getCalRightBottom1()
{
	return (&cal_data)->right_bottom[1];
}

/* Returns the 2st value for right bottom of calibration data
*/ 
jint Java_iEpi_Scale_BoardInterface_getCalRightBottom2()
{
	return (&cal_data)->right_bottom[2];
}

/* Returns the 0st value for left bottom of calibration data
*/ 
jint Java_iEpi_Scale_BoardInterface_getCalLeftBottom0()
{
	return (&cal_data)->left_bottom[0];
}

/* Returns the 1st value for left bottom of calibration data
*/ 
jint Java_iEpi_Scale_BoardInterface_getCalLeftBottom1()
{
	return (&cal_data)->left_bottom[1];
}

/* Returns the 2st value for left bottom of calibration data
*/ 
jint Java_iEpi_Scale_BoardInterface_getCalLeftBottom2()
{
	return (&cal_data)->left_bottom[2];
}