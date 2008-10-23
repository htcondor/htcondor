package condor.gahp;

public class VersionHandler implements CommandHandler{
    private GahpInterface gahp = null;
    public void setGahp (GahpInterface g) {
        this.gahp = g;
    }

    public CommandHandlerResponse handleCommand (String[] command) {
        return new CommandHandlerResponse (
                    CommandHandlerResponse.SUCCESS + 
                    " $GahpVersion: 2.0.0 Jul 30 2008 "+
                    IOUtils.escapeWord(gahp.getVersion())+" $");
    }
}
