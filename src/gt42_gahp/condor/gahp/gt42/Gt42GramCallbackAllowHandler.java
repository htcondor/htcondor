package condor.gahp.gt42;

import condor.gahp.*;


public class Gt42GramCallbackAllowHandler implements CommandHandler {
    private GahpInterface gahp;
    public void setGahp (GahpInterface g) {
        gahp = g;
    }

    public CommandHandlerResponse handleCommand (String[] cmd) {
        Integer reqId = null;
        Integer port = null;    
        try {
            reqId = new Integer(cmd[1]);
			if (cmd.length > 2)
				port = new Integer(cmd[2]);
        }  catch (Exception e) {
            e.printStackTrace(System.err);
            return CommandHandlerResponse.SYNTAX_ERROR;
        }

		CallbackSink callbackSink;
		String callbackID;

		try {
			callbackSink = new CallbackSink(reqId.intValue(), port, gahp);
			callbackID = gahp.generateUniqueId();

			CallbackSink.storeCallbackSink (gahp, callbackID, callbackSink);
			gahp.addCleanupStep ((CleanupStep)callbackSink);
		} catch (Exception e) {
            e.printStackTrace(System.err);
            return new CommandHandlerResponse (
                                               CommandHandlerResponse.FAILURE+" 1 "+IOUtils.escapeWord(e.getMessage()));
        }

        return new CommandHandlerResponse (
            CommandHandlerResponse.SUCCESS + " " + callbackID);
    }    


}
 
