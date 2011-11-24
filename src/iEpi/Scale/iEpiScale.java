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
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * This class provides iEpiScale activity. 
 * 
 * @author Mohammad
 */
public class iEpiScale extends Activity 
{
	//
	// IDs for dynamically created objects
	//
	private final int 			txtStatusID 		= 100;
	/**
	 * Maximum number of trials to start reading the weight data from the board.
	 */
	private final int			MAX_READ_TRIAL		= 2;
	/**
	 * Maximum number of trials to read the calibration data from the board. 
	 */
	private final int 			MAX_CAL_TRIAL 		= 2;
	/**
	 * Used to format the result of weight calculation to two decimal-point format
	 */
	DecimalFormat 				dfmTwoDecimalFormat	= new DecimalFormat("#.##");
	/**
	 * Represents the revision number of the current code.
	 */
	private int 				intRevision 		= 48;
	/**
	 * UI container of the current activity.
	 */
	private LinearLayout 		ui;
	/**
	 * This flag is used by the thread which is in charge of reading weight data from the board. When set to false, 
	 * the thread stops reading data.
	 */
	private static boolean 		blnShouldStop		= false;
	/**
	 * Tag for error log outputs.	
	 */
	final static String 		LOG_TAG 			= "iEpiScale";
	
	/**
	 * BoardInterface object which allows this activity to connect to the board.
	 */
	static BoardInterface		boardInterface;
	
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
    
    /**
     * Initializes the view of the current activity
     */
	private void initializeViewIfNecessary() 
	{
		// Quit if the ui has already been generated.
		if (ui != null)
			return;

		// Create the ui.
		ScrollView scrollView = new ScrollView(this);
		ui = new LinearLayout(this);
		ui.setOrientation(LinearLayout.VERTICAL);
		scrollView.addView(ui);
		setContentView(scrollView);

		ConnectKey();
		ConnectOnlyKey();
		CalibrateKey();
		ReadKey();
		DisconnectKey();
		
		TextView txtStatus = new TextView(this);
		txtStatus.setId(txtStatusID);
		ui.addView(txtStatus);
	}
	
	public void ConnectOnlyKey() 
	{
		Button btnConnectOnly = new Button(this);
		btnConnectOnly.setText("Connect Only");
		btnConnectOnly.setOnClickListener(new View.OnClickListener() 
		{
			public void onClick(View v) 
			{
				int result = boardInterface.intConnect(3);
				((TextView)findViewById(txtStatusID)).setText("Returned connect result is: " +  result);
			}
		});
		ui.addView(btnConnectOnly);
	}

	public void CalibrateKey() 
	{
		Button btnCalibrate = new Button(this);
		btnCalibrate.setText("Calibrate");
		btnCalibrate.setOnClickListener(new View.OnClickListener() 
		{
			public void onClick(View v) 
			{
				int result = boardInterface.getCalibrationData();
				((TextView)findViewById(txtStatusID)).setText("Returned calibrate result is: " +  result);
			}
		});
		ui.addView(btnCalibrate);
	}

	public void ReadKey() 
	{
		Button btnRead = new Button(this);
		btnRead.setText("Read");
		btnRead.setOnClickListener(new View.OnClickListener() 
		{
			public void onClick(View v) 
			{
				int result = boardInterface.startReadingData();
				((TextView)findViewById(txtStatusID)).setText("Returned read result is: " +  result);
			}
		});
		ui.addView(btnRead);
	}
	
	public void ConnectKey() 
	{
		Button btnConnect = new Button(this);
		btnConnect.setText("Connect V" + intRevision);
		btnConnect.setOnClickListener(new View.OnClickListener() 
		{
			public void onClick(View v) 
			{
				if(blnIsConnected)
				{
					((TextView)findViewById(txtStatusID)).setText("You are already connected to a board.");
					return;
				}
				
				new AlertDialog.Builder(v.getContext())
					.setTitle("Confirm")
					.setMessage("Please activate the board by pressing the sync button, and then click \"OK\" to continue?")
					.setPositiveButton("OK", new DialogInterface.OnClickListener() 
											{
												public void onClick(DialogInterface dialog, int which) 
												{
													// Connect to the board
													((TextView)findViewById(txtStatusID)).setText("Connecting ...");
													int result = boardInterface.ConnectCalibrateRead(3);
													if(result <= -1)
													{
														((TextView)findViewById(txtStatusID)).setText(
																"Could not connect to Bluetooth. Please try again.\n" + 
																"If this problem happens again, please restart the application.\n" + 
																"Error No: " +  result);
													}
													else if(result == 1)
													{
														((TextView)findViewById(txtStatusID)).setText("Connection successful.");
														PostConnectionProcess();
													}
													else
													{
														((TextView)findViewById(txtStatusID)).setText("Unknown error type.");
													}
												}
											})
					.setNegativeButton("Cancel", null)
					.show();
			}
		});
		ui.addView(btnConnect);
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
	
	public void DisconnectKey()
	{
		Button btnDisconnect = new Button(this);
		btnDisconnect.setText("Disconnect");
		ui.addView(btnDisconnect);
		btnDisconnect.setOnClickListener(new View.OnClickListener() 
		{
			public void onClick(View v) 
			{
				if(!blnIsConnected)
				{
					((TextView)findViewById(txtStatusID)).setText("You are not currently connected to the board.");
					return;
				}
				
				// First stop reading from the board ...
				Log.d(LOG_TAG,"Set the flag to stop the thread!");
				blnShouldStop = true;
				boardInterface.stopReadingData();
				
				// Now disconnect from the board ...
				
				Disconnect();
				blnIsConnected = false;
			}
		});		
	}    

	private void Disconnect()
	{
		int result = boardInterface.disconnect();
		if(result == -1)
			((TextView)findViewById(txtStatusID)).setText("Disconnection failed.");
		else if(result == 0)
			((TextView)findViewById(txtStatusID)).setText("Disconnected successfully.");					
		else
			((TextView)findViewById(txtStatusID)).setText("Unknown error type.");
		
        PackageManager packageManager = getPackageManager();

        Intent surveyScreenIntent = packageManager.getLaunchIntentForPackage("com.surveyapp");

		    //pass in a file name and timeout time in milliseconds here
		String surveyID = "loopexamplesurvey.xml";
		surveyScreenIntent.putExtra("com.iSurveyor.SurveyXML", surveyID);
		int timeOutInMS = -1; //-1 indicates NO timeout
		surveyScreenIntent.putExtra("com.iSurveyor.SurveyTimeOutInMS", timeOutInMS);
		startActivityForResult(surveyScreenIntent, 0);		
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
			if(totalWeight != -1)
				((TextView)findViewById(txtStatusID)).setText("The current weight is: " + dfmTwoDecimalFormat.format(totalWeight));
			else
				((TextView)findViewById(txtStatusID)).setText("Balance data is not valid.");
	    }
	}
	
	public void onPause()
	{
		super.onPause();
		Log.d(LOG_TAG, "onPause called.");
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
			boardInterface.disconnect();
			boardInterface = null;
		}
	}
	
	public void onDestroy()
	{
		super.onDestroy();
		Log.d(LOG_TAG, "onDestroy called.");
		blnShouldStop = true;
		
		if(boardInterface != null)
		{
			boardInterface.disconnect();
			boardInterface = null;
		}
	}
}