package condor.gahp.gt42;

import condor.gahp.*;

import org.globus.exec.utils.FaultUtils;
import org.oasis.wsrf.faults.BaseFaultType;
import org.globus.exec.generated.StateEnumeration;
import org.globus.exec.client.GramJob;
import org.ietf.jgss.GSSCredential;


public class Gt42GramJobStatusHandler implements CommandHandler {

    private GahpInterface gahp;

    public void setGahp (GahpInterface gahp) {
        this.gahp = gahp;
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
            new JobStatusRunnable(reqId.intValue(), contactString, gahp,
                                  proxy) );
    } // handleCommand

    class JobStatusRunnable implements Runnable {
        private int requestId;
        private String jobHandle;
        private GahpInterface gahp;
        private GSSCredential proxy;

        public JobStatusRunnable (int reqId, String jobHandle,
                                  GahpInterface gahp, GSSCredential proxy) {
            this.requestId = reqId;
            this.jobHandle = jobHandle;
            this.gahp = gahp;
            this.proxy = proxy;
        }

        public void run() {
            // Job status code
            String statusString = null;
            String faultString = null;
            int exitCode = -1;

            try {

				GramJob gramJob = new GramJob();
				gramJob.setHandle (this.jobHandle);
				
					// Set default attributes
				GramJobUtils.setDefaultJobAttributes (gramJob, this.proxy);

					// Get the state
				gramJob.refreshStatus();
				StateEnumeration jobState = 
					gramJob.getState();

                statusString = jobState.getValue();

                if (jobState.equals (StateEnumeration.Failed)) {
                    BaseFaultType[] jobFaults;
                    jobFaults = gramJob.getFault();
                    if ( jobFaults != null ) {
                        // TODO should print out full fault text somewhere
                        //   (stderr?)
                        faultString = FaultUtils.getErrorMessageFromFaults(jobFaults);
                    } else {
                        faultString = "NULL";
                    }
                }

                exitCode = gramJob.getExitCode();

            } catch (Exception e) {
                System.err.println("status failed: ");
                e.printStackTrace(System.err);
                // TODO: Improve this shit
                String errorMessage = (e.getMessage()==null)?"unknown":e.getMessage();
                String [] result = {
                    ""+1, //result code
                    "NULL", // job state
                    "NULL", // job fault string
                    "NULL", // job exit code
                    errorMessage}; // error message
                gahp.addResult (requestId, result); 
                return;
            }

            String[] result = 
                { "0", // result code
                  statusString, // jobState
                  faultString, // job fault string
                  ""+exitCode, // job exit code
                  "NULL" // error message
            };

            gahp.addResult (requestId, result);

        }
    }
} 
