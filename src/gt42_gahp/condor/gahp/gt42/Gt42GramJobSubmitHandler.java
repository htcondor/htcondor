package condor.gahp.gt42;

import condor.gahp.*;

import java.util.Calendar;
import java.util.Date;
import java.net.URL;
import org.globus.exec.client.GramJob;
import org.globus.exec.utils.client.ManagedJobFactoryClientHelper;
import org.globus.axis.message.addressing.EndpointReferenceType;
import org.ietf.jgss.GSSCredential;


public  class Gt42GramJobSubmitHandler implements CommandHandler {
    private GahpInterface gahp;
    
    public void setGahp (GahpInterface g) {
        gahp = g;
    }

    public CommandHandlerResponse handleCommand (String[] cmd) {

        Integer reqId;
		String submitId;
        String contactString = null;
		String jobmanagerType = null;
        String callbackContact = null;
        String RSL = null;
		Integer termTime = null;
        CallbackSink callbackSink = null;
        

        try {
            // cmd[0] = GRAM_JOB_REQUEST
            reqId = new Integer(cmd[1]);
			submitId = cmd[2];
            contactString = cmd[3];
			jobmanagerType = cmd[4];
            if ( cmd[5].length() > 0 && !cmd[5].equals( "NULL" ) ) {
                callbackContact = cmd[5];
            }
            RSL = cmd[6];
			if (cmd.length > 7 && !cmd[7].equals( "NULL" ) ) {
				termTime = new Integer(cmd[7]);
			}
        }
        catch (Exception e) {
            e.printStackTrace(System.err);
            return CommandHandlerResponse.SYNTAX_ERROR;
        }

        if (callbackContact != null && callbackContact.length() > 0) {
            // Check that callbackcontact is valid
            callbackSink = CallbackSink.getCallbackSink(gahp, callbackContact);
            if (callbackSink == null) {
                // TODO: is this right? What if GAHP was restarted? maybe we would create a new one?
                System.err.println ("Unable to find notification sink manager "+callbackContact);
                return CommandHandlerResponse.SYNTAX_ERROR;
            }
        }

        // grab the currently-active proxy
        GSSCredential proxy = GSIUtils.getCredential (gahp);

        MyTask toRun = new MyTask (reqId,
								   submitId, 
								   RSL, 
								   contactString,
								   jobmanagerType,
								   callbackSink, 
								   gahp,
								   termTime,
                                   proxy);

        return new CommandHandlerResponse (CommandHandlerResponse.SUCCESS,
                                           toRun);
    }

    class MyTask implements Runnable {
            
		
        //public final GramJob job;
        public final String factoryAddr;
		public final String factoryType;
        public final String jobRSL;

        public final Integer requestId;
		public final String submitId;
        private CallbackSink callbackSink;

        private boolean fullDelegation;
        private String jobHandle = null;

        private GahpInterface gahp;
		private Integer termTime;
        private GSSCredential proxy;

        public MyTask(Integer requestId,
					  String submitId,
                         String rsl, 
                         String factoryAddr, 
						 String factoryType,
                         CallbackSink callbackSink,
                         GahpInterface gahp,
					  Integer termTime,
                      GSSCredential proxy) {
            this.factoryAddr = factoryAddr;
			this.factoryType = factoryType;
            this.jobRSL = rsl;
            this.requestId = requestId;
			this.submitId = submitId;
            this.gahp = gahp;
            this.fullDelegation = true;
            this.callbackSink = callbackSink;
			this.termTime = termTime;
            this.proxy = proxy;
        }
            
        public void run () {
            try {
                jobHandle = submitJob ();

            } catch (Exception e) {
                System.err.println ("ERROR Submitting Job: " + e.getMessage());
                e.printStackTrace(System.err);
                // TODO: We gota be more specific than that, yo...
                String errorString = e.toString();
                if (errorString == null) errorString = "unknown";
                gahp.addResult (
                        requestId.intValue(),
                        new String[] { "1", "NULL", errorString}
                        ); 
                return;
            }

            // Announce the result
            gahp.addResult (
                            requestId.intValue(),
                            new String[] { "0", this.jobHandle, "NULL"}
                            );

        } // run

        private String submitJob() throws Exception
		{
			
				// Create the factory
			URL factoryURL = ManagedJobFactoryClientHelper.
				getServiceURL(factoryAddr).getURL();

			EndpointReferenceType factoryEndpoint =
				ManagedJobFactoryClientHelper.getFactoryEndpoint(
					factoryURL,
					factoryType);	

				// Create the job with the given RSL
			GramJob gramJob = new GramJob (this.jobRSL);

				// Set default attributes
			GramJobUtils.setDefaultJobAttributes (gramJob, this.proxy);

				// Set termination time
            Date dateTermTime = null;
            if ( termTime != null ) {
                long term_time = termTime.intValue();
                term_time *= 1000;
                dateTermTime = new Date( term_time );
            } else {
                Calendar calTermTime = Calendar.getInstance();
                calTermTime.add (Calendar.HOUR, 12);
                dateTermTime = calTermTime.getTime();
            }
// workaround to avoid null-pointer exception in gramJob.setTerminationTime()
//gramJob.setDuration(0,0);
			gramJob.setTerminationTime (dateTermTime);
			
            gramJob.setDelegationEnabled( false );

			gramJob.submit (
						factoryEndpoint,
						true, // "batch" mode -> do not bind()	
						false, // limited delegation
						submitId);


				// Register with the callback listener
				// Notice that this may cause a race condition,
				// (e.g. notification being delivered before we're
				// subscribed)....
				// But since we poll the status anyway, I think that's ok

			String jobHandle = gramJob.getHandle();
            if ( this.callbackSink != null ) {
                this.callbackSink.addJobListener (gramJob, jobHandle);
            }
			
			return jobHandle;
		}
        
    } // class MyTask

} // class GRAM_JOB_SUBMIT_Handler
