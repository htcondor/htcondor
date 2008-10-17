package condor.gahp.gsi;

import condor.gahp.*;

import java.io.File;
import java.io.FileInputStream;
import java.io.ByteArrayOutputStream;


import org.ietf.jgss.GSSCredential;
import org.ietf.jgss.GSSException;



public class CacheProxyFromFileHandler implements CommandHandler {

    private GahpInterface gahp;
    
    public void setGahp (GahpInterface gahp) {
        this.gahp = gahp;
    }
    
    public CommandHandlerResponse handleCommand (String[] cmd) {
        
        String id = null;
        String fileName = null;       

        try {
            id = cmd[1];
            fileName = cmd[2];
            if (id.length() == 0)
                throw new Exception ("Invalid id "+ id);
        }
        catch (Exception e) {
            e.printStackTrace(System.err);
            return CommandHandlerResponse.SYNTAX_ERROR;
        }
        
        if (id.equalsIgnoreCase ("DEFAULT")) {
            return new CommandHandlerResponse (CommandHandlerResponse.FAILURE + ' ' + IOUtils.escapeWord("DEFAULT not allowed"));
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
        
        GSIUtils.cacheCredential (gahp, id, cred);
        return new CommandHandlerResponse (CommandHandlerResponse.SUCCESS);
        
    } // handleCommand
   
}
