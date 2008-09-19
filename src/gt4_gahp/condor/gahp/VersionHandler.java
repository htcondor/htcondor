package condor.gahp;

public class VersionHandler implements CommandHandler{
    private GahpInterface gahp = null;
    public void setGahp (GahpInterface g) {
        this.gahp = g;
    }

    public CommandHandlerResponse handleCommand (String[] command) {
        return new CommandHandlerResponse (
                    CommandHandlerResponse.SUCCESS + 
                    " $GahpVersion: 1.7.2 Jul 30 2008 "+
                    IOUtils.escapeWord(gahp.getVersion())+" $");
    }
}
