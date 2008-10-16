package condor.gahp.gt42;

import condor.gahp.*;

import org.ietf.jgss.GSSCredential;
import org.globus.exec.client.GramJob;


public  class Gt42GramJobStartHandler implements CommandHandler {
    private GahpInterface gahp;
    
        public void setGahp (GahpInterface g) {
            gahp = g;
        }

    public CommandHandlerResponse handleCommand (String[] cmd) {
            Integer reqId;
            String contactString = null;
            String callbackContact = null;
            boolean fullDelegation = false;

            try {
				reqId = new Integer(cmd[1]);
				contactString = cmd[2];

            }
            catch (Exception e) {
				e.printStackTrace(System.err);
				return CommandHandlerResponse.SYNTAX_ERROR;
            }

            GSSCredential proxy = GSIUtils.getCredential (gahp);

            StartGramJob toRun = new StartGramJob (reqId, 
                                               contactString, 
                                               fullDelegation,
                                               gahp, proxy);

            return new CommandHandlerResponse (
                    CommandHandlerResponse.SUCCESS,
                    toRun);
    }

    class StartGramJob implements Runnable {
		public final String jobHandle;
		public final Integer requestID;
		private String callbackContact;
		private GahpInterface gahp;
		private boolean fullDelegation;
        private GSSCredential proxy;
            
		public StartGramJob (Integer requestID, 
							 String _jobHandle, 
							 boolean fullDelegation,
							 GahpInterface gahp,
                             GSSCredential proxy) {
			this.jobHandle = _jobHandle;
			this.requestID = requestID;
			this.gahp = gahp;
			this.fullDelegation = fullDelegation;
            this.proxy = proxy;
		}
            
		public void run () {
			try {
				startJob ();
			}
			catch (Exception e) {
				System.err.println ("Error starting job");
				e.printStackTrace(System.err);

				String errorString = e.toString();
				if (errorString == null) errorString = "unknown";
				gahp.addResult (
								requestID.intValue(),
								new String[] { "1", errorString}
								); 
				return;
			}

				// Job started successfully
			gahp.addResult (
							requestID.intValue(),
							new String [] { "0", "NULL" });

		} // run

     
        private boolean startJob() throws Exception
		{
			GramJob gramJob = new GramJob();
			gramJob.setHandle (this.jobHandle);
			
			GramJobUtils.setDefaultJobAttributes (gramJob, this.proxy);

			gramJob.release();

			return true;
		}
    } // class StartGramJob
} 
