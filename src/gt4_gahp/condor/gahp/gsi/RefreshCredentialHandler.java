package condor.gahp.gsi;

import condor.gahp.*;
import condor.gahp.gsi.GSIUtils;

public  class RefreshCredentialHandler implements CommandHandler {
    private GahpInterface gahp;
    
        public void setGahp (GahpInterface g) {
            gahp = g;
        }

    public CommandHandlerResponse handleCommand (String[] cmd) {
        Integer reqId;
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


			// This is currently a no-op

            
		return new CommandHandlerResponse (CommandHandlerResponse.SUCCESS);
    }
}
 
