package condor.gahp;

import java.util.LinkedList;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Vector;
import java.util.List;


import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.PrintStream;
import java.io.FileOutputStream;
import java.io.FileInputStream;

//import condor.gahp.gsi.*;
//import condor.gahp.gass.*;
import condor.gahp.gt42.*;

import java.net.URL;

public class Gahp implements GahpInterface {

    public static final String VERSION = "GT42 GAHP (GT-4.2.0)";

    protected Hashtable commandDispatchTable = new Hashtable();
    protected CommandResultQueue resultQueue = new CommandResultQueue();
    protected Vector cleanupSteps = new Vector();

    protected static int uniqueIdSeed = 0;
    protected Hashtable commonObjectTable = new Hashtable();

    protected PrintStream stdout;

    protected boolean isAsyncMode = false;
    protected boolean hasNewResults = false;

    protected boolean quitSignaled = false;

    private String printPrefix = "";
    private String newPrintPrefix = null;

    public static void main(String[] args) throws Exception {

        Gahp gahp = new Gahp();
    
        // GAHP output
        gahp.stdout = System.out; // (this should be /dev/stdout)

        System.setOut (System.err);
    
        // GAHP input
        BufferedReader stdin = new BufferedReader (new InputStreamReader(System.in));

        // Print the version
        String[] version = gahp.processCommand ("VERSION");
        synchronized (gahp.stdout) {
            gahp.stdout.println (version[0].substring(2)); // Remove leading "S "
        }

        // This is the main command loop
        while(!gahp.quitSignaled) {
        
            // Read command
            String line = null;
            try {
                line = stdin.readLine();
            }
            catch (java.io.IOException e) {
                e.printStackTrace(System.err);
                synchronized (gahp.stdout) {
                    gahp.stdout.println (gahp.printPrefix+CommandHandlerResponse.ERROR);
                }
                continue;
            }

            if (line == null)  {
                gahp.requestExit();
                continue;
            }

            // Parse the line
            String [] result = gahp.processCommand (line);
            synchronized (gahp.stdout) {
                for (int i=0; i<result.length; i++) {
                    gahp.stdout.println (gahp.printPrefix+result[i]);
                }

                // The reply to the RESPONSE_PREFIX command can't use the
                // new prefix, but all lines afterwards must. Therefore, we
                // set the new printPrefix after the reply but before any
                // other lines are printed (thus inside the synchronized
                // block).
                if ( gahp.newPrintPrefix != null ) {
                    gahp.printPrefix = gahp.newPrintPrefix;
                    gahp.newPrintPrefix = null;
                }
            }

        } // elihw

        gahp.shutdown();

    }

    public Gahp() {
        commandDispatchTable.put ("RESULTS", new ResultsHandler());

        commandDispatchTable.put ("QUIT", new QuitHandler());
        commandDispatchTable.put ("VERSION", new VersionHandler());
        commandDispatchTable.put ("COMMANDS", new CommandsHandler());
/*
        commandDispatchTable.put ("GT42_GRAM_CALLBACK_ALLOW", new Gt42GramCallbackAllowHandler());
        commandDispatchTable.put ("GT42_GRAM_JOB_SUBMIT", new Gt42GramJobSubmitHandler());
        commandDispatchTable.put ("GT42_GRAM_JOB_START", new Gt42GramJobStartHandler());
        commandDispatchTable.put ("GT42_GRAM_JOB_STATUS", new Gt42GramJobStatusHandler());
        commandDispatchTable.put ("GT42_GRAM_JOB_DESTROY", new Gt42GramJobCancelHandler());
        commandDispatchTable.put ("INITIALIZE_FROM_FILE", new InitializeFromFileHandler());
        commandDispatchTable.put ("GT42_GRAM_CALLBACK_ALLOW", new Gt42GramCallbackAllowHandler());
        commandDispatchTable.put ("GT42_GRAM_JOB_CALLBACK_REGISTER", new Gt42GramJobCallbackRegisterHandler());
		commandDispatchTable.put ("GT42_GENERATE_SUBMIT_ID", new Gt42GenerateSubmitIdHandler());
        commandDispatchTable.put ("GT42_DELEGATE_CREDENTIAL", new DelegateCredentialHandler());
        commandDispatchTable.put ("GT42_REFRESH_CREDENTIAL", new RefreshCredentialHandler());
        commandDispatchTable.put ("GT42_DELEGATE_CREDENTIAL_2", new DelegateCredentialHandler());
        commandDispatchTable.put ("GT42_REFRESH_CREDENTIAL_2", new RefreshCredentialHandler());
        commandDispatchTable.put ("GT42_SET_TERMINATION_TIME", new Gt42SetTerminationTimeHandler());
        commandDispatchTable.put ("GT42_SET_TERMINATION_TIME_2", new Gt42SetTerminationTimeHandler());
        commandDispatchTable.put ("GT42_GRAM_PING", new Gt42GramPingHandler());
*/
        // Support GRAM 4.0 names, but still using GRAM 4.2 protocol
        commandDispatchTable.put ("GT4_GRAM_CALLBACK_ALLOW", new Gt42GramCallbackAllowHandler());
        commandDispatchTable.put ("GT4_GRAM_JOB_SUBMIT", new Gt42GramJobSubmitHandler());
        commandDispatchTable.put ("GT4_GRAM_JOB_START", new Gt42GramJobStartHandler());
        commandDispatchTable.put ("GT4_GRAM_JOB_STATUS", new Gt42GramJobStatusHandler());
        commandDispatchTable.put ("GT4_GRAM_JOB_DESTROY", new Gt42GramJobCancelHandler());
        commandDispatchTable.put ("GT4_GRAM_CALLBACK_ALLOW", new Gt42GramCallbackAllowHandler());
        commandDispatchTable.put ("GT4_GRAM_JOB_CALLBACK_REGISTER", new Gt42GramJobCallbackRegisterHandler());
		commandDispatchTable.put ("GT4_GENERATE_SUBMIT_ID", new Gt42GenerateSubmitIdHandler());
        commandDispatchTable.put ("GT4_DELEGATE_CREDENTIAL", new DelegateCredentialHandler());
        commandDispatchTable.put ("GT4_REFRESH_CREDENTIAL", new RefreshCredentialHandler());
        commandDispatchTable.put ("GT4_DELEGATE_CREDENTIAL_2", new DelegateCredentialHandler());
        commandDispatchTable.put ("GT4_REFRESH_CREDENTIAL_2", new RefreshCredentialHandler());
        commandDispatchTable.put ("GT4_SET_TERMINATION_TIME", new Gt42SetTerminationTimeHandler());
        commandDispatchTable.put ("GT4_SET_TERMINATION_TIME_2", new Gt42SetTerminationTimeHandler());
        commandDispatchTable.put ("GT4_GRAM_PING", new Gt42GramPingHandler());
////////////
        commandDispatchTable.put ("ASYNC_MODE_ON", new AsyncModeOnHandler());
        commandDispatchTable.put ("ASYNC_MODE_OFF", new AsyncModeOnHandler());

        commandDispatchTable.put ("CACHE_PROXY_FROM_FILE", new CacheProxyFromFileHandler());
        commandDispatchTable.put ("UNCACHE_PROXY", new UncacheProxyHandler());
        commandDispatchTable.put ("USE_CACHED_PROXY", new UseCachedProxyHandler());
        commandDispatchTable.put ("REFRESH_PROXY_FROM_FILE", new RefreshProxyFromFileHandler());
        commandDispatchTable.put ("RESPONSE_PREFIX", new ResponsePrefixHandler());
        
        //\ Set pointers to this Gahp server for all handlers
        for (Iterator iter=commandDispatchTable.values().iterator(); iter.hasNext(); ) {
            ((CommandHandler)iter.next()).setGahp ((GahpInterface)this);
        }


			// If we get shutdown irregular manner (e.g. SIGTERM)
		Runtime.getRuntime().addShutdownHook (
			new Thread (
						new Runnable() {
							public void run() {
								doCleanupSteps();
							}
						}
						));
    }

    protected synchronized void setAsyncMode (boolean mode) {
        synchronized (this.resultQueue) {
            isAsyncMode=mode;
            if (isAsyncMode) {
                hasNewResults = false;
            }
        }
    }

    public void addResult(int reqId, String[] result) {
        synchronized (this.resultQueue) {
            if (hasNewResults == false && isAsyncMode == true) {
                // We've just added a new result, signal
                synchronized (stdout) {
                    stdout.println (printPrefix+"R");
                }
            }
        
            hasNewResults = true;
        
            this.resultQueue.addResult (reqId, result);
        } // synchronized
    }

    public String getVersion () {
        return VERSION;
    }

    protected void setPrintPrefix (String new_prefix) {
        newPrintPrefix = new_prefix;
    }

    // Dispatch the command to the right handler
    // Return the return line
    protected String[] processCommand(String line) {

        String [] command = parseCommand (line);

        if (command == null || command.length == 0) 
            return CommandHandlerResponse.SYNTAX_ERROR.returnLine;
        
        // Figure out handler for this command
        CommandHandler handler = null;
        if (command != null) {
            // Convert command name to uppercase, so that it is case-insensitive
            command[0] = command[0].toUpperCase(); 
            handler = (CommandHandler)commandDispatchTable.get (command[0]);
        }

        if (handler == null) {
            return CommandHandlerResponse.SYNTAX_ERROR.returnLine;
        }

        // Invoke the handler (this should return immediately)
        // This gives you back a return line and possibly a Runnable object
        CommandHandlerResponse response = handler.handleCommand (command);
        
        // If there is a Runnable returned, spawned a new thread with it
        if (response.toRun != null) {
            // QUESTION: Should there be a upper limit on conncurrent threads?
            new Thread(response.toRun).start();
        }

        // Return return line
        return response.returnLine;
    }

    protected String[] parseCommand(String cmd) {
        Vector args=new Vector();
        StringBuffer currentArg = new StringBuffer();

        for (int i=0; i<cmd.length(); i++) {
            char c = cmd.charAt(i);

            if ((c == '\\')                 // if this char = '\' 
                && (i<cmd.length()-1)       // and there is a next char 
                && ((cmd.charAt (i+1) == ' ') || 	// and the next char is a space
					(cmd.charAt (i+1) == '\n') ||
					(cmd.charAt (i+1) == '\r'))	// or a newline
				)
            {  									// then it must be an embedded space,
                currentArg.append(cmd.charAt (i+1));     // so just it as part of this agrument
                i++;                        			// and skip processing the next char 
            } else if (c == ' ' || c == '\t' || c == '\r')  { // Found true separator char
                args.add (currentArg.toString());   // so add the current argument
                currentArg.setLength(0);            // and reset the buffer
            } else {
                //plain old char   
                currentArg.append (c);
            }
        }

        if (currentArg.length() > 0) {  
            // Add the last argument (since there usually isn't a termination char after it)
            args.add (currentArg.toString());
            currentArg.setLength(0);
        }

        // Copy into array
        String [] result = new String[args.size()];
        args.copyInto (result);
        return result;
    }

	public void doCleanupSteps() {
		synchronized (cleanupSteps) {
            for (int i=0; i<cleanupSteps.size(); i++) {
                CleanupStep step = (CleanupStep)cleanupSteps.get(i);
                step.doCleanup();
            }
			cleanupSteps.clear();
		}
	}

    private void shutdown() {
		doCleanupSteps();

		if (this.quitSignaled) {
			System.exit(0);
		}

    }

    // Notify the gahp that to exit
    // E.g. the QUIT command handler calls this
    public void requestExit() {
        quitSignaled = true;
    }

    public String[] getCommands() {
        Vector v = new Vector();
        v.addAll (commandDispatchTable.keySet());
        java.util.Collections.sort(v);
        String[] result = new String[v.size()];
        v.copyInto (result);
        return result;
    }

    public void addCleanupStep(CleanupStep step) {
        synchronized (cleanupSteps) {
            cleanupSteps.add (step);
        }
    }

    public Object getObject(String key) {
        synchronized (commonObjectTable) {
            return this.commonObjectTable.get(key);
        }
    }

    public Object storeObject(String key, Object object) {
        synchronized (commonObjectTable) {
            Object lastObject = this.commonObjectTable.get(key);
            this.commonObjectTable.put(key, object);
            return lastObject;
        }

    }

    public Object removeObject(String key) {
        synchronized (commonObjectTable) {
            return this.commonObjectTable.remove(key);
        }
    }

    public String generateUniqueId() {
        String uniqueId;
        synchronized (this) {
            uniqueId= ""+(++uniqueIdSeed);
        }
        return uniqueId;
    }

    class NOOP_Handler implements CommandHandler {
        public void setGahp (GahpInterface gahp) {
            // Not needed since we're an inner class
        }

        public CommandHandlerResponse handleCommand (String[] command) {
            return new CommandHandlerResponse(
                                              CommandHandlerResponse.SUCCESS, null);
        }

    }

    class AsyncModeOffHandler implements CommandHandler {

        public void setGahp (GahpInterface g) {
        }

        public CommandHandlerResponse handleCommand (String[] cmd) {
            setAsyncMode (false);
            return new CommandHandlerResponse (CommandHandlerResponse.SUCCESS);
        } // handleCommand
    }

    class AsyncModeOnHandler implements CommandHandler {

        public void setGahp (GahpInterface g) {
        }

        public CommandHandlerResponse handleCommand (String[] cmd) {
            setAsyncMode (true);
            return new CommandHandlerResponse (CommandHandlerResponse.SUCCESS);
        } // handleCommand
}

    class ResponsePrefixHandler implements CommandHandler {
        public void setGahp (GahpInterface gahp) {
        }
        public CommandHandlerResponse handleCommand (String[] cmd) {
            if (!(cmd.length == 2 && cmd[0].equals("RESPONSE_PREFIX"))) {
                return CommandHandlerResponse.SYNTAX_ERROR;
            }
            setPrintPrefix( cmd[1] );
            return new CommandHandlerResponse(CommandHandlerResponse.SUCCESS);
        }
    }

    class ResultsHandler implements CommandHandler{
        public void setGahp (GahpInterface gahp) {
            // Not needed since we're an inner class
        }
        
        public CommandHandlerResponse handleCommand (String[] command) {
            if (command.length != 1 && command[0].equals("RESULTS")) {
	       	return CommandHandlerResponse.SYNTAX_ERROR;
            }

            Vector result = new Vector();

            // This is only possible b/c we're an inner class
            CommandResultQueue.CommandResult[] results =  null;
            synchronized (resultQueue) {
                results = resultQueue.flushResultList();
                hasNewResults=false;
            }

            // First line is # of words
            result.add (CommandHandlerResponse.SUCCESS+" "+results.length);

            // Go though each result and escape each word
            for (int i=0; i<results.length; i++) {
                StringBuffer resultLineBuff = new StringBuffer();
                resultLineBuff.append (results[i].requestID);

                for (int j=0; j<results[i].result.length; j++) {
                    resultLineBuff.append (' ');
                    resultLineBuff.append (
                        IOUtils.escapeWord (results[i].result[j]));
                }
                result.add (resultLineBuff.toString());
            }
                
            String [] resultArray = new String[result.size()];
            result.copyInto (resultArray);

            return new CommandHandlerResponse (resultArray, null);
        }
    }
} // class Gahp

