package condor.gahp.gt42;

import condor.gahp.*;

import java.util.List;
import java.util.Iterator;
import org.ietf.jgss.GSSCredential;
import org.globus.exec.client.GramJob;


public class Gt42GramJobCancelHandler implements CommandHandler {

    private GahpInterface gahp = null;

    public void setGahp (GahpInterface g) {
        gahp = g;
    }
    
    public CommandHandlerResponse handleCommand (String[] cmd) {
        Integer reqId = null;
        String contactString = null;

        try {
            // cmd[0] = GRAM_JOB_REQUEST
            reqId = new Integer(cmd[1]);
            contactString = cmd[2];
        }
        catch (Exception e) {
            e.printStackTrace(System.err);
            return CommandHandlerResponse.SYNTAX_ERROR;
        }
        
        GSSCredential proxy = GSIUtils.getCredential (gahp);

        return new CommandHandlerResponse (
            CommandHandlerResponse.SUCCESS,
            new JobCancelRunnable(reqId.intValue(), contactString, gahp, proxy) );
    } // handleCommand
    

    class JobCancelRunnable implements Runnable {
        private int requestId;
        private String jobHandle;
        private GahpInterface gahp;
        private GSSCredential proxy;

        public JobCancelRunnable (int reqId, String handle, GahpInterface gahp,
                                  GSSCredential proxy) {
            this.requestId = reqId;
            this.jobHandle = handle;
			this.gahp = gahp;
            this.proxy = proxy;
        }
        
        public void run() {
            try {
                // Remove the job listener from callback sinks
                // This has to happen before we cancel the job.
                // Otherwise, the attempt to destroy the subscription
                // source on the server will fail and the server will
                // start to accumulate old junk. So sayeth the Globus
                // people.
                // If we destroy the subscription after the job, then
                // we get a NoSuchResourceException!?!?!?
				List callbackSinkList = 
					CallbackSink.getAllCallbackSinks (this.gahp);
				for (Iterator iter = callbackSinkList.iterator();
					 iter.hasNext(); ) {
					((CallbackSink)iter.next()).removeJobListener (
							   this.jobHandle);
				}

				GramJob gramJob = new GramJob();
				gramJob.setHandle (this.jobHandle);
				
					// Set default attributes
				GramJobUtils.setDefaultJobAttributes (gramJob, this.proxy);

                // If we don't set this, gramJob.cancel() will destroy the
                // delegated credential, which is shared with other jobs
                gramJob.setDelegationEnabled( false );

					// Cancel the job
				gramJob.terminate(true, false, false);

            } catch (Exception e) {
                System.err.println("cancel failed: ");
                e.printStackTrace(System.err);
                
                String errorMessage = 
					(e.getMessage()==null)?"unknown":e.getMessage();
                String [] result = {
                    ""+1, //result code
                    errorMessage}; // job state

                gahp.addResult (requestId, result);
				return;
            }
            
            String[] result = { "0", "NULL" };
            gahp.addResult (requestId, result);

        }
    }
} // GRAM_JOB_CANCEL_Handler
