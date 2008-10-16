package condor.gahp.gt42;

import condor.gahp.*;



public class UncacheProxyHandler implements CommandHandler {

    private GahpInterface gahp;
    
    public void setGahp (GahpInterface gahp) {
        this.gahp = gahp;
    }
    
    public CommandHandlerResponse handleCommand (String[] cmd) {
        
        String id = null;
        
        try {
            id = cmd[1];
            if (id.length() == 0)
                throw new Exception ("Invalid id "+ id);
        }
        catch (Exception e) {
                e.printStackTrace(System.err);
                return CommandHandlerResponse.SYNTAX_ERROR;
        }

        try {
            GSIUtils.deleteCredential (gahp, id);
        }
        catch (Exception e) {
            e.printStackTrace(System.err);
            return new CommandHandlerResponse(CommandHandlerResponse.FAILURE);
        }
        
        return new CommandHandlerResponse (CommandHandlerResponse.SUCCESS);
        
    } // handleCommand
   
}
