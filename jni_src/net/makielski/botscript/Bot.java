package net.makielski.botscript;

/**
 * Wrapper class for a Bot Script Bot object. 
 */
public class Bot {
	
    static {
    	// Load the shared library.
        System.loadLibrary("bs_lib");
        
        // Start the IO service thread.
		new Thread() {
			@Override
			public void run() {
				Bot.runService();
			}
		}.start();
    }
    
	/**
	 * Asynchronous callback handler interface.
	 */
	public static interface AsyncHandler {
		/**
		 * This function implements the reaction to the handler invocation.
		 * 
		 * @param error the result of the asynchronous operation
		 */
		void call(String error);
	}


	/** Memory holding the bot. */
	public long bot;
	
	/** Log and status change update handler. */
	private AsyncHandler updateHandler;
	
	/**
	 * Runs the Bot Script library Boost Asio io_service.
	 */
	public static native void runService();
	
	
	/**
	 * Loads the packages from the filesystem.
	 * 
	 * @return all packages
	 */
	public static native String[] loadPackages(String path);
	
	/**
	 * Creates an identifier.
	 * 
	 * @param u the login username
	 * @param p the package name
	 * @param s the server
	 * @return the identifier
	 */
	public static native String createIdentifier(String u, String p, String s);
	
	/**
	 * Creates a new bot object.
	 * 
	 * @param m the bot motor
	 */
	public Bot(AsyncHandler updateHandler) {
		this.updateHandler = updateHandler;
		bot = createBot(updateHandler);
	}
	
	/**
	 * Loads the specified configuration and calls 
	 * 
	 * @param config JSON formatted bot configuration
	 * @param handler completion handler
	 */
	public native void load(String config, AsyncHandler handler);
	
	/** @return the identifier */
	public native String identifier();
	
	/** @return the username */
	public native String u();
	
	/** @return the package */
	public native String p();
	
	/** @return the server */
	public native String s();
	
	/** @return the configuration */
	public native String configuration(boolean includePassword);
	
	/**
	 * Executes the specified command. The command has the following structure:
	 * 
	 * <p><code>modulename_set_statuskey</code></p>
	 * 
	 * <p>For example: <code>"base_set_proxy"</code> to set the proxy.</p>
	 * 
	 * @param command the command to execute
	 * @param argument the command argument
	 */
	public native void execute(String command, String argument);

	/**
	 * Let the native code allocate some memory.
	 *  
	 * @param updateHandler the update handler to set
	 */
	private native long createBot(AsyncHandler updateHandler);

	/** Shuts the bot down. */
	public native void shutdown();
}
