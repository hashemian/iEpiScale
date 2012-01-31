/*
 *  iEpiScale
 *
 *  Copyright (C) 2011 Mohammad Hashemian (m.hashemian@gmail.com)
 *   
 *  This code represents an activity which uses BoardInterface in order
 *  to connect Android to Wii Balance Board. The application is intended 
 *  to use for iEpi study.
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

package iEpi.Scale;

import java.text.DecimalFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.bluetooth.BluetoothAdapter;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.TextView;

import java.io.*;

import junit.framework.Assert;

/**
 * This class provides iEpiScale activity. 
 * 
 * @author Mohammad
 */
public class iEpiScale extends Activity 
{
	/**
	 * Used to format the result of weight calculation to two decimal-point format
	 */
	private DecimalFormat 		dfmTwoDecimalFormat	= new DecimalFormat("#.##");
	/**
	 * Used to format the current time down to hour accuracy for file storage.
	 */
	private SimpleDateFormat	sdfFileDateFormat = new SimpleDateFormat("yyyyMMddhh0000");
	/**
	 * Used to format the current time down to second accuracy to write in the file.
	 */
	private SimpleDateFormat	sdfStorageDateFormat = new SimpleDateFormat("yyyyMMddhhmmss");
	/**
	 * Represents the revision number of the current code.
	 */
	private int 				intRevision 		= 04;
	/**
	 * This flag is used by the thread which is in charge of reading weight data from the board. When set to false, 
	 * the thread stops reading data.
	 */
	private static boolean 		blnShouldStop		= false;
	/**
	 * Tag for error log outputs.	
	 */
	final static 	String 		LOG_TAG 			= "iEpiScale";
	/**
	 * GUI elements
	 */
	private			Button		btnConnect;
	private 		Button		btnDisconnect;
	private			Button		btnSurvey;
	private			RadioButton	rbtnKg;
	private 		EditText	txtResult;
	private			CheckBox	chkNotRecord;
	private			TextView	txtvInfo;
	/**
	 * BoardInterface object which allows this activity to connect to the board.
	 */
	static BoardInterface		boardInterface;
	/**
	 * The conversion ratio between KG and Lbs.
	 */
	static final double 		KG_TO_LBS_RATIO = 2.20462; 
	
	/**
	 * The following arrays hold the calibration data for the board. Each cell of the board has 3 
	 * calibration values, being held at the relevant array.
	 */
	private int[] 				calibrationDataLT 	= new int[3];
	private int[] 				calibrationDataLB 	= new int[3];
	private int[] 				calibrationDataRT 	= new int[3];
	private int[] 				calibrationDataRB 	= new int[3];
	
	/**
	 * Determines whether the program is already connected to a board (true) or not (false).
	 */
	private boolean				blnIsConnected		= false;
	private ProgressDialog 		prgrsDialog;
	public final Handler 		mHandler 			= new Handler();
	/**
	 * Bluetooth MAC address as long. This is used as device identifier during data recording.
	 */
	private long				lngMacAddress;
	/**
	 * Determines the path to the location where the saved weights have to be stored. 
	 */
	private static final String	DATA_STORAGE_PATH 	= "/sdcard/.healthlogger/DumpedFiles/"; // TODO: This path has to be fixed before deployment.
	/**
	 * Determines the file extension for the saved files. 
	 */
	private static final String	DATA_FILE_EXT		= ".scaledat";
	/**
	 * Interval between each weight update operation in milliseconds
	 */
	private static final int	WEIGHT_UPDATE_INTERVAL = 3000;
	/**
	 * 
	 */
	private static final int	MIN_TOTAL_WEIGHT_FROM_SENSORS = -50;
	/**
	 * Called when the activity is first created. 
	 */
    public void onCreate(Bundle savedInstanceState) 
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        // Set up the application's view.
		initializeViewIfNecessary();
        
		if(boardInterface == null)
			boardInterface = new BoardInterface();
		else 
			Log.d(LOG_TAG,"The Native Bluetooth Interface object already exist!");
		
		// Retrieving device MAC address and recording it ...
		BluetoothAdapter btAdapter = BluetoothAdapter.getDefaultAdapter();
		if(btAdapter == null)
		{
			deactivateRecording();
		}
		else
		{
			try
			{
				this.lngMacAddress = convertMacToLong(btAdapter.getAddress());
				if(this.lngMacAddress == 0)
					deactivateRecording();
			}
			catch(Exception ex)
			{
				System.out.println(ex.getMessage());
			}
		}
    }
    
    /**
     * Deactivates saving the recorded information. This happens mainly if the process of retrieving MAC address as device identifier fails.
     */
    private void deactivateRecording()
    {
    	this.chkNotRecord.setChecked(false);
    	this.chkNotRecord.setEnabled(false);
    	this.txtvInfo.setText(R.string.WeightNotSavedError);
    }
    
    /**
     * Initializes the view of the current activity
     */
	private void initializeViewIfNecessary() 
	{
		btnConnect = (Button) findViewById(R.id.btnConnect);
		btnDisconnect = (Button) findViewById(R.id.btnDisconnect);
		btnSurvey = (Button) findViewById(R.id.btnSurvey);
		rbtnKg = (RadioButton) findViewById(R.id.rbtnKg);
		txtResult = (EditText) findViewById(R.id.result);
		chkNotRecord = (CheckBox) findViewById(R.id.chkNotRecord);
		txtvInfo = (TextView) findViewById(R.id.txtvInfo);

		btnConnect.setOnClickListener(ConnectKey);
		btnDisconnect.setOnClickListener(DisconnectKey);
		btnSurvey.setOnClickListener(launchSurvey);

		txtResult.setKeyListener(null);

		// Connect to the board
		//txtResult.setText("Connecting ...");
		prgrsDialog = new ProgressDialog(this);
		prgrsDialog.setMessage(getResources().getText(R.string.ConnectionProgressDialogMessage));
	}
	
	private OnClickListener launchSurvey = new OnClickListener()
	{
		public void onClick(View v)
		{
			PackageManager packageManager = getPackageManager();

	        Intent surveyScreenIntent = packageManager.getLaunchIntentForPackage("com.surveyapp");

			// pass in a file name and timeout time in milliseconds here
			String surveyID = "loopexamplesurvey.xml";
			surveyScreenIntent.putExtra("com.iSurveyor.SurveyXML", surveyID);
			int timeOutInMS = -1; //-1 indicates NO timeout
			surveyScreenIntent.putExtra("com.iSurveyor.SurveyTimeOutInMS", timeOutInMS);
			startActivityForResult(surveyScreenIntent, 0);		
		}
	};
	
	private OnClickListener ConnectKey = new OnClickListener() 
	{
		public void onClick(View v)
		{
			if(blnIsConnected)
			{
				txtvInfo.setText(R.string.AlreadyConnectedMessage);
				return;
			}
			
			AlertDialog.Builder alertConnectionConfirmation = new AlertDialog.Builder(iEpiScale.this);
			alertConnectionConfirmation.setMessage(getResources().getString(R.string.ConnectionConfirmationMessage) + Integer.toString(intRevision));
			alertConnectionConfirmation.setPositiveButton("OK", new DialogInterface.OnClickListener() 
				{
					public void onClick(DialogInterface dialog, int which) 
					{
						dialog.cancel();
						prgrsDialog.show();
						Thread t =new Thread(new Runnable() 
							{
								public void run() 
								{
									final int result = boardInterface.ConnectCalibrateRead(3);
									mHandler.post(new Runnable()
										{
											public void run()
											{
												setTextView(result);
											}
										});
									prgrsDialog.dismiss();
								}
							});
						t.start();
					}
				});
			alertConnectionConfirmation.setNegativeButton("Cancel", null);
			alertConnectionConfirmation.show();												
		}
	};
	
	private void setTextView(int result) 
	{
		if(result == -7)
		{
			txtvInfo.setText(getResources().getText(R.string.LowBatteryErrorMessage) +  
					"Error No: " + result);
		}
		else if(result <= -1)
		{
			txtvInfo.setText(getResources().getText(R.string.GeneralErrorMessage) +
					"Error No: " + result);
		}
		else if(result == 1)
		{
			txtvInfo.setText(R.string.ConnectionSuccessful);
			PostConnectionProcess();
		}
		else
		{
			txtvInfo.setText(R.string.UnknownErrorMessage);
		}
	}
	
	public void PostConnectionProcess()
	{
		calibrationDataLT[0] = boardInterface.getCalLeftTop0();
		calibrationDataLT[1] = boardInterface.getCalLeftTop1();
		calibrationDataLT[2] = boardInterface.getCalLeftTop2();
		
		calibrationDataLB[0] = boardInterface.getCalLeftBottom0();
		calibrationDataLB[1] = boardInterface.getCalLeftBottom1();
		calibrationDataLB[2] = boardInterface.getCalLeftBottom2();
		
		calibrationDataRT[0] = boardInterface.getCalRightTop0();
		calibrationDataRT[1] = boardInterface.getCalRightTop1();
		calibrationDataRT[2] = boardInterface.getCalRightTop2();
		
		calibrationDataRB[0] = boardInterface.getCalRightBottom0();
		calibrationDataRB[1] = boardInterface.getCalRightBottom1();
		calibrationDataRB[2] = boardInterface.getCalRightBottom2();
			
		Log.d(LOG_TAG,"Going to start the thread!");
		blnShouldStop = false;
		new WeightRep().execute();
		blnIsConnected = true;
	}
	
	private OnClickListener DisconnectKey = new OnClickListener()
	{
		public void onClick(View v) 
		{
			blnShouldStop = true;
			
			// Now disconnect from the board ...
			
			Disconnect();
			blnIsConnected = false;
		}
	};    

	private void Disconnect()
	{
		int result = boardInterface.disconnect();
		if(result == -1)
			txtvInfo.setText(R.string.DisconnectionFailed);
		else if(result == 0 || result == 1)
			txtvInfo.setText(R.string.DisconnectionSuccessful);					
		else
			txtvInfo.setText(R.string.UnknownErrorMessage);		
	}
	
	private class WeightRep extends AsyncTask<Void,Void,Void>
	{
		double totalWeight = 0.0;
		
		public WeightRep()
		{
			blnShouldStop = false;
		}

		@Override
		protected Void doInBackground(Void... params) 
		{
			try
			{
				while(!blnShouldStop)
				{
					UpdateWeight();
					publishProgress();
					Thread.sleep(WEIGHT_UPDATE_INTERVAL);
				}
			}
			catch(InterruptedException ex)
			{
				Log.d(LOG_TAG,"ERROR: " + ex.toString());
			}
			return null;
		}

		public void UpdateWeight()
		{
			if(boardInterface.getIsBalanceDataValid() == 1)
			{
				int tlValue = boardInterface.getTopLeftValue();
				int trValue = boardInterface.getTopRightValue();
				int blValue = boardInterface.getBottomLeftValue();
				int brValue = boardInterface.getBottomRightValue();
				
				double tlWeight, trWeight, blWeight, brWeight;
				if(tlValue < calibrationDataLT[1])
				{
					tlWeight = 0 + 
						(((((double)tlValue - (double)calibrationDataLT[0]) * 17.0) - 
						  (((double)tlValue - (double)calibrationDataLT[0]) *  0.0)) / 
								((double)calibrationDataLT[1] - (double)calibrationDataLT[0]));
				}
				else
				{
					tlWeight = 17.0 + 
					(((((double)tlValue - (double)calibrationDataLT[1]) * 34.0) - 
					  (((double)tlValue - (double)calibrationDataLT[1]) * 17.0)) / 
							((double)calibrationDataLT[2] - (double)calibrationDataLT[1]));
				}
				
				if(trValue < calibrationDataRT[1])
				{
					trWeight = 0 + 
					(((((double)trValue - (double)calibrationDataRT[0]) * 17.0) - 
							  (((double)trValue - (double)calibrationDataRT[0]) *  0.0)) / 
									((double)calibrationDataRT[1] - (double)calibrationDataRT[0]));
				}
				else
				{
					trWeight = 17.0 + 
					(((((double)trValue - (double)calibrationDataRT[1]) * 34.0) - 
							  (((double)trValue - (double)calibrationDataRT[1]) * 17.0)) / 
									((double)calibrationDataRT[2] - (double)calibrationDataRT[1]));
				}
				
				if(blValue < calibrationDataLB[1])
				{
					blWeight = 0 + 
					(((((double)blValue - (double)calibrationDataLB[0]) * 17.0) - 
							  (((double)blValue - (double)calibrationDataLB[0]) *  0.0)) / 
									((double)calibrationDataLB[1] - (double)calibrationDataLB[0]));
				}
				else
				{
					blWeight = 17.0 + 
					(((((double)blValue - (double)calibrationDataLB[1]) * 34.0) - 
							  (((double)blValue - (double)calibrationDataLB[1]) * 17.0)) / 
									((double)calibrationDataLB[2] - (double)calibrationDataLB[1]));
				}
				
				if(brValue < calibrationDataRB[1])
				{
					brWeight = 0 + 
					(((((double)brValue - (double)calibrationDataRB[0]) * 17.0) - 
							  (((double)brValue - (double)calibrationDataRB[0]) *  0.0)) / 
									((double)calibrationDataRB[1] - (double)calibrationDataRB[0]));
				}
				else
				{
					brWeight = 17.0 + 
					(((((double)brValue - (double)calibrationDataRB[1]) * 34.0) - 
							  (((double)brValue - (double)calibrationDataRB[1]) * 17.0)) / 
									((double)calibrationDataRB[2] - (double)calibrationDataRB[1]));
				}
				
				totalWeight = trWeight + tlWeight + brWeight + blWeight;				
			}
			else
			{
				totalWeight = -1;
			}
		}
		
		protected void onProgressUpdate(Void... params) 
		{
			try
			{
				int intScaleResourceId;
				double weightToDisplay = totalWeight;
				if(rbtnKg.isChecked())
					intScaleResourceId = R.string.kilogram;
				else
				{
					intScaleResourceId = R.string.pound;
					weightToDisplay *= KG_TO_LBS_RATIO;
				}
				if(totalWeight != -1)
				{
					if(totalWeight < MIN_TOTAL_WEIGHT_FROM_SENSORS)
					{
						blnShouldStop = true;
						boardInterface.disconnect();
						txtvInfo.setText(R.string.LowBatteryErrorMessage);
					}
					else
					{
						txtResult.setText(dfmTwoDecimalFormat.format(weightToDisplay) + " " + getResources().getString(intScaleResourceId));
						if(!chkNotRecord.isChecked())
						{
							Date dtCurrentTime = Calendar.getInstance().getTime();
							File flRecord = new File(DATA_STORAGE_PATH + 
														Long.toString(lngMacAddress) + "-" + 
														sdfFileDateFormat.format(dtCurrentTime) + 
														DATA_FILE_EXT);
							FileWriter fwStorage = new FileWriter(flRecord, true);
							fwStorage.append(sdfStorageDateFormat.format(dtCurrentTime) + "\t" + totalWeight + "\n");
							fwStorage.close();
						}
					}
				}
				else
				{
					txtResult.setText("");
					txtvInfo.setText(R.string.InvalidBalanceData);
				}
			}
			catch(IOException ioex)
			{
				Log.d(LOG_TAG, "Error: " +  ioex.getMessage());
			}
	    }
	}
	
	public void onPause()
	{
		super.onPause();
		Log.d(LOG_TAG, "onPause called.");
		if(boardInterface != null)
		{
			Log.d(LOG_TAG, "onPause: Interface is not null. Going to call disconnect!");
			boardInterface.disconnect();
			boardInterface = null;
		}
		else
			Log.d(LOG_TAG, "onPause: Interface was null. Skipped disconnection.");
		finish();
	}
	
	public void onResume()
	{
		super.onResume();
		Log.d(LOG_TAG, "onResume called.");
	}
	
	public void onStop()
	{
		super.onStop();
		Log.d(LOG_TAG, "onStop called");
		blnShouldStop = true;
		
		if(boardInterface != null)
		{
			Log.d(LOG_TAG, "onStop: Interface is not null. Going to call disconnect!");
			boardInterface.disconnect();
			boardInterface = null;
		}
		else
			Log.d(LOG_TAG, "onStop: Interface was null. Skipped disconnection.");
		finish();
	}
	
	public boolean onKeyDown(int keyCode, KeyEvent event)
	{
	    if ((keyCode == KeyEvent.KEYCODE_BACK) || (keyCode == KeyEvent.KEYCODE_HOME))
	    {
			if(boardInterface != null)
			{
				Log.d(LOG_TAG, "onKeyDown: Interface is not null. Going to call disconnect!");
				boardInterface.disconnect();
				boardInterface = null;
			}
			else
				Log.d(LOG_TAG, "onKeyDown: Interface was null. Skipped disconnection.");
	        finish();
	    }
	    return super.onKeyDown(keyCode, event);
	}
	
	public void onDestroy()
	{
		super.onDestroy();
		Log.d(LOG_TAG, "onDestroy called.");
		blnShouldStop = true;
		
		if(boardInterface != null)
		{
			if(blnIsConnected)
			{
				Log.d(LOG_TAG, "onDestroy: Interface is not null. Going to call disconnect!");
				boardInterface.disconnect();
			}
			boardInterface = null;
		}
		else
			Log.d(LOG_TAG, "onDestroy: Interface was null. Skipped disconnection.");
	}
	
	public static final long convertMacToLong(String mac)
	{
		try
		{
			// Partial precondition check; other problems will be caught by the decoding process.
			Assert.assertNotNull(mac);
			
			// Convert from a long into a mac address. Any problem will be found during the decoding process.
			String rawLowercaseHex = mac.replaceAll(":", "").toLowerCase();
			Long macLong = Long.decode("#" + rawLowercaseHex);
			
			// Confirm we have the same number.
			String reconvertedLowercaseHex = Long.toHexString(macLong);
			Assert.assertTrue("Could not reconvert to MAC from long. Reconverted: " + 
					reconvertedLowercaseHex + 
					"; Given value, less separators: " + 
					rawLowercaseHex, 
					rawLowercaseHex.contains(reconvertedLowercaseHex));
			
			return macLong;
		}
		catch(Exception ex)
		{
			return 0;
		}
	}
}