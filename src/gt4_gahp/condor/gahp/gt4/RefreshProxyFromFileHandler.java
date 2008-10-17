package condor.gahp.gt4;

import condor.gahp.*;

import java.io.File;
import java.io.FileInputStream;
import java.io.ByteArrayOutputStream;

import org.ietf.jgss.GSSCredential;
import org.ietf.jgss.GSSException;

public class RefreshProxyFromFileHandler implements CommandHandler {

    private GahpInterface gahp;
    
    public void setGahp (GahpInterface gahp) {
        this.gahp = gahp;
    }
    
    public CommandHandlerResponse handleCommand (String[] cmd) {
        String fileName = null;

        try {
            fileName = cmd[1];
        }
        catch (Exception e) {
            e.printStackTrace(System.err);
            return CommandHandlerResponse.SYNTAX_ERROR;
        }
        
        File file = new File (fileName);
        if (!(file.exists() && file.canRead())) {
            return new CommandHandlerResponse (CommandHandlerResponse.FAILURE, null);
        }

        GSSCredential cred = null;
        
        try {
            cred = GSIUtils.readCredentialFromFile(fileName);
        } catch (Exception e) {
            System.err.println("Error reading credential: " + e.getMessage());
            e.printStackTrace(System.err);
            return new CommandHandlerResponse (CommandHandlerResponse.FAILURE + 
                " " + 
                IOUtils.escapeWord (e.getMessage()));
        }
        
        GSIUtils.setDefaultCredential (gahp, cred);
        return new CommandHandlerResponse (CommandHandlerResponse.SUCCESS);
    } // handleCommand
   
}
