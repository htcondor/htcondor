package condor.gahp;

public class VersionHandler implements CommandHandler{
    private GahpInterface gahp = null;
    public void setGahp (GahpInterface g) {
        this.gahp = g;
    }

    public CommandHandlerResponse handleCommand (String[] command) {
        return new CommandHandlerResponse (
                    CommandHandlerResponse.SUCCESS + 
                    " $GahpVersion: 2.0.2 Nov 11 2009 "+
                    IOUtils.escapeWord(gahp.getVersion())+" $");
    }
}
