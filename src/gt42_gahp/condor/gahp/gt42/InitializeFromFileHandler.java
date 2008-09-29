package condor.gahp.gt42;

import condor.gahp.*;

import org.ietf.jgss.GSSCredential;

import org.gridforum.jgss.ExtendedGSSCredential;
import org.gridforum.jgss.ExtendedGSSManager;


public class InitializeFromFileHandler implements CommandHandler {

    private GahpInterface gahp;
    
    public void setGahp (GahpInterface gahp) {
        this.gahp = gahp;
    }
    
    public CommandHandlerResponse handleCommand (String[] cmd) {
        
        String fileName = null;

        try {
            // cmd[0] = GRAM_JOB_REQUEST
            fileName = cmd[1];
        }
        catch (Exception e) {
            e.printStackTrace(System.err);
            return CommandHandlerResponse.SYNTAX_ERROR;
        }

		GSSCredential proxy = null;

		try {

			ExtendedGSSManager manager = 
				(ExtendedGSSManager)ExtendedGSSManager.getInstance();
			String handle = "X509_USER_PROXY="+ fileName;

			proxy =
				manager.createCredential(handle.getBytes(),
					 ExtendedGSSCredential.IMPEXP_MECH_SPECIFIC,
										 GSSCredential.DEFAULT_LIFETIME, 
										 null,
										 GSSCredential.INITIATE_AND_ACCEPT);

		}
		catch (Exception e) {
			e.printStackTrace(System.err);
			return new CommandHandlerResponse (CommandHandlerResponse.FAILURE);
		}

        GSIUtils.setDefaultCredential (gahp, proxy);
        return new CommandHandlerResponse (CommandHandlerResponse.SUCCESS);
        
    } // handleCommand
   
}
