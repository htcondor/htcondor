package condor.gahp.gt42;

import condor.gahp.*;

import org.ietf.jgss.GSSCredential;
import org.globus.exec.client.GramJob;


public  class Gt42GramJobCallbackRegisterHandler implements CommandHandler {
    private GahpInterface gahp;
    
    public void setGahp (GahpInterface g) {
        gahp = g;
    }

    public CommandHandlerResponse handleCommand (String[] cmd) {
        Integer reqId;
        String contactString = null;
        String callbackContact = null;

        try {
            // cmd[0] = 
            reqId = new Integer(cmd[1]);
            contactString = cmd[2];
            callbackContact = cmd[3];
        }
        catch (Exception e) {
            e.printStackTrace(System.err);
            return CommandHandlerResponse.SYNTAX_ERROR;
        }

        // Check that callbackcontact is valid
        if (CallbackSink.getCallbackSink(gahp, callbackContact) == null) {
            // TODO: is this right? What if GAHP was restarted? maybe we would create a new one?
            System.err.println ("Unable to find notification sink manager "+callbackContact);
            return CommandHandlerResponse.SYNTAX_ERROR;
        }

        // TODO:
        // Use callback contact
        RegisterJobCallbackRunnable toRun =
            new RegisterJobCallbackRunnable (reqId,
                                             contactString,
                                             callbackContact,
                                             gahp);

        return new CommandHandlerResponse (CommandHandlerResponse.SUCCESS,
                                           toRun);
    }

    class RegisterJobCallbackRunnable implements Runnable {

        public final String jobHandle;
        public final Integer requestID;
        private String callbackContact;
        private GahpInterface gahp;

        public RegisterJobCallbackRunnable(Integer requestID, 
                                           String jobHandle, 
                                           String callbackContact,
                                           GahpInterface gahp) {
            this.jobHandle = jobHandle;
            this.requestID = requestID;
            this.gahp = gahp;
            this.callbackContact = callbackContact;
        }

        public void run () {
            String [] result = null;
            try {
				GramJob gramJob = new GramJob();
				gramJob.setHandle (jobHandle);
				
					// Retrieve the proxy
				GSSCredential proxy = GSIUtils.getCredential(this.gahp);
				gramJob.setCredentials (proxy);
				
					// Get a callbacksink
                CallbackSink sink =
                    CallbackSink.getCallbackSink(gahp, callbackContact);

                // TODO This JobListener get unregistered only if a cancel
                //   command is issued. Otherwise, they start accumulating
                //   over time. Need a way to remove old ones we don't care
                //   about anymore. Is there a way for the GAHP server to
                //   reliably figure this out for itself, or will it require
                //   a new "cleanup" command from the gahp client?

					// Add this job to our callback sink
                sink.addJobListener (gramJob, jobHandle);

					// If we haven't Excepted by now, we're peachy
                result = new String [] { "0", "NULL" };

            }
            catch (Exception e) {
                System.err.println ("Error registering for callbacks");
                e.printStackTrace(System.err);
                String errorString = e.toString();
                if (errorString == null) errorString = "unknown";
                result = new String[] { "1", errorString};
            }

            gahp.addResult( requestID.intValue(), result );
        }

    }
    
}
