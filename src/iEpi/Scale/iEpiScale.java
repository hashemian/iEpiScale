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

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
//import android.content.Intent;
//import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioButton;

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
	DecimalFormat 				dfmTwoDecimalFormat	= new DecimalFormat("#.##");
	/**
	 * Represents the revision number of the current code.
	 */
	private int 				intRevision 		= 93;
	/**
	 * This flag is used by the thread which is in charge of reading weight data from the board. When set to false, 
	 * the thread stops reading data.
	 */
	private static boolean 		blnShouldStop		= false;
	/**
	 * Tag for error log outputs.	
	 */
	final static String 		LOG_TAG 			= "iEpiScale";
	private	Button				btnConnect;
	private Button				btnDisconnect;
	private Button				btnConnectOnly;
	private Button				btnCalibrate;
	private Button				btnRead;
	private	RadioButton			rbtnKg;
	private EditText			txtResult;
	/**
	 * BoardInterface object which allows this activity to connect to the board.
	 */
	static BoardInterface		boardInterface;
	/**
	 * 
	 */
	static final double KG_TO_LBS_RATIO = 2.20462; 
	
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
    }
    
	private void setTextView(int result) 
	{
		if(result == -7)
		{
			txtResult.setText("Battery level of the board is very low. Please replace board's batteries.\n" + 
					"Error No: " + result);
		}
		else if(result <= -1)
		{
			txtResult.setText("Could not connect to Bluetooth. Please try again.\n" + 
					"If this problem happens again, please restart the application.\n" + 
					"Error No: " + result);
		}
		else if(result == 1)
		{
			txtResult.setText("Connection successful.");
			PostConnectionProcess();
		}
		else
		{
			txtResult.setText("Unknown error type.");
		}
	}
    /**
     * Initializes the view of the current activity
     */
	private void initializeViewIfNecessary() 
	{
		btnConnect = (Button) findViewById(R.id.btnConnect);
		btnDisconnect = (Button) findViewById(R.id.btnDisconnect);
		btnConnectOnly = (Button) findViewById(R.id.btnConnectOnly);
		btnCalibrate = (Button) findViewById(R.id.btnCalibrate);
		btnRead = (Button) findViewById(R.id.btnRead);
		rbtnKg = (RadioButton) findViewById(R.id.rbtnKg);
		txtResult = (EditText) findViewById(R.id.result);
				
		btnConnect.setOnClickListener(ConnectKey);
		btnDisconnect.setOnClickListener(DisconnectKey);
		
		btnConnectOnly.setOnClickListener(ConnectOnlyKey);
		btnConnectOnly.setEnabled(false);
		
		btnCalibrate.setOnClickListener(CalibrateKey);
		btnCalibrate.setEnabled(false);
		
		btnRead.setOnClickListener(ReadKey);
		btnRead.setEnabled(false);
		
		txtResult.setKeyListener(null);
		
		// Connect to the board
		txtResult.setText("Connecting ...");
		prgrsDialog = new ProgressDialog(this);
		prgrsDialog.setMessage("Connecting ...\nPlease do not stand on the board while this message is shown.");
	}
	
	private OnClickListener ConnectKey = new OnClickListener() 
	{
		public void onClick(View v)
		{
			if(blnIsConnected)
			{
				txtResult.setText("You are already connected to a board.");
				return;
			}
			
			AlertDialog.Builder alertConnectionConfirmation = new AlertDialog.Builder(iEpiScale.this);
			alertConnectionConfirmation.setMessage("Please activate the board by pressing the sync button, and then click \"OK\" to continue?" + intRevision);
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
			txtResult.setText("Disconnection failed.");
		else if(result == 0)
			txtResult.setText("Disconnected successfully.");					
		else
			txtResult.setText("Unknown error type.");
		
/*        PackageManager packageManager = getPackageManager();

        Intent surveyScreenIntent = packageManager.getLaunchIntentForPackage("com.surveyapp");

		// pass in a file name and timeout time in milliseconds here
		String surveyID = "loopexamplesurvey.xml";
		surveyScreenIntent.putExtra("com.iSurveyor.SurveyXML", surveyID);
		int timeOutInMS = -1; //-1 indicates NO timeout
		surveyScreenIntent.putExtra("com.iSurveyor.SurveyTimeOutInMS", timeOutInMS);
		startActivityForResult(surveyScreenIntent, 0);*/		
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
					Thread.sleep(2000);
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
			String strScale;
			double weightToDisplay = totalWeight;
			if(rbtnKg.isChecked())
				strScale = " KG";
			else
			{
				strScale = " Lbs";
				weightToDisplay *= KG_TO_LBS_RATIO;
			}
			if(totalWeight != -1)
				txtResult.setText("The current weight is: " + dfmTwoDecimalFormat.format(weightToDisplay) + strScale);
			else
				txtResult.setText("Balance data is not valid.");
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
	
	private OnClickListener ConnectOnlyKey = new OnClickListener() 
	{
		public void onClick(View v)
		{
			int result = boardInterface.intConnect(3);
			txtResult.setText("Returned connect result is: " +  result);
		}
	};

	private OnClickListener CalibrateKey = new OnClickListener() 
	{
		public void onClick(View v)
		{
			int result = boardInterface.getCalibrationData();
			txtResult.setText("Returned calibrate result is: " +  result);
		}
	};

	private OnClickListener ReadKey = new OnClickListener() 
	{
		public void onClick(View v)
		{
			int result = boardInterface.startReadingData();
			txtResult.setText("Returned read result is: " +  result);
			blnShouldStop = false;
			new WeightRep().execute();
			blnIsConnected = true;
		}
	};

}