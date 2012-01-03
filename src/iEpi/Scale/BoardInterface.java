package iEpi.Scale;

/**
 * This class provides an interface between native C code which is in charge of connection to the 
 * balance board, and the iEpiScale activity which acts as the user interface.
 * 
 * @author mohammad
 *
 */
public class BoardInterface 
{
	private static final String LOG_TAG = "BoardInterface";
	
	// -- import native code -- // 
	/**
	 * Retrieves the version information from the native module
	 */
	public native String	intReadVersion		( );
	/**
	 * Combines the operations of connection, calibration, and data acquisition. 
	 * @param scantime
	 * @return
	 * 1 	if the operation is successful,
	 * -1	In case of a general error,
	 * -2	If the number of found BT devices are negative,
	 * -3	If Wii connection creation fails,
	 * -4	In case of the failure to open connection
	 * -5	In case of the failure to find any BT device
	 * -6	In case of the failure to create the connection.
	 */
	public native int 		ConnectCalibrateRead( int scantime );
	/**
	 * Searches for the balance board and if it finds any, it tries to connect to the board.
	 * @param scantime
	 * @return 
	 * -3 if connection creation fails
	 * -2 if the number of found devices are negative,
	 * 0 if there is no Bluetooth device in proximity
	 * 1 if the connection to the board established successfully. 
	 */
	public native int		intConnect			( int scantime );
	/**
	 * Retrieves the calibration data from the board and fills the relevant data structures.
	 * @return
	 * 0 if the operation is successful, -1 otherwise.
	 */
	public native int	 	getCalibrationData	( );
	/**
	 * Sets the report mode of the board to continuously report the weight. This causes the board to continuously
	 * report it's state to the phone. 
	 * 
	 * @return
	 * -1 if setting the report mode fails, 0 if the operation finishes successfully.
	 */
	public native int	 	startReadingData	( );
	/**
	 * Stops the device from reporting the weight continuously.
	 */
	public native void 		stopReadingData		( );
	/**
	 * Disconnects from the board and releases the created resources.
	 * @return 0 if the device disconnects the board successfully, -1 otherwise.
	 */
	public native int 		disconnect			( );

	/**
	 * Returns whether balance board calibration values are valid or not
	 * @return 1 if the calibration data are valid, 0 otherwise.
	 */
	public native int	 	getIsCalibrationDataValid();
	/**
	 * Returns the 0st value for right top of calibration data
	 * @return
	 */
	public native int	 	getCalRightTop0();
	/**
	 * Returns the 1st value for right top of calibration data
	 * @return
	 */
	public native int	 	getCalRightTop1();
	/**
	 * Returns the 2nd value for right top of calibration data
	 * @return
	 */	
	public native int	 	getCalRightTop2();
	/**
	 * Returns the 0st value for left top of calibration data
	 * @return
	 */
	public native int	 	getCalLeftTop0();
	/**
	 * Returns the 1st value for left top of calibration data
	 * @return
	 */
	public native int	 	getCalLeftTop1();
	/**
	 * Returns the 2nd value for left top of calibration data
	 * @return
	 */
	public native int	 	getCalLeftTop2();
	/**
	 * Returns the 0st value for right bottom of calibration data
	 * @return
	 */
	public native int	 	getCalRightBottom0();
	/**
	 * Returns the 1st value for right bottom of calibration data
	 * @return
	 */
	public native int	 	getCalRightBottom1();
	/**
	 * Returns the 2nd value for right bottom of calibration data
	 * @return
	 */
	public native int	 	getCalRightBottom2();
	/**
	 * Returns the 0st value for left bottom of calibration data
	 * @return
	 */
	public native int	 	getCalLeftBottom0();
	/**
	 * Returns the 1st value for left bottom of calibration data
	 * @return
	 */
	public native int	 	getCalLeftBottom1();
	/**
	 * Returns the 2nd value for left bottom of calibration data
	 * @return
	 */
	public native int	 	getCalLeftBottom2();
	
	/**
	 * Returns whether balance board weight values are valid or not
	 * @return
	 */
	public native int	 	getIsBalanceDataValid();
	/**
	 * Returns the top right value of the board
	 * @return
	 */
	public native int	 	getTopRightValue();
	/**
	 * Returns the top left value of the board
	 * @return
	 */
	public native int	 	getTopLeftValue();
	/**
	 * Returns the bottom right value of the board
	 * @return
	 */
	public native int	 	getBottomRightValue();
	/**
	 * Returns the bottom left value of the board
	 * @return
	 */
	public native int	 	getBottomLeftValue();

	public BoardInterface()
	{	}
	
	static 
	{
		System.loadLibrary("BTL");
	}
}
