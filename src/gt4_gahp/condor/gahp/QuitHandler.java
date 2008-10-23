package condor.gahp;

public class QuitHandler implements CommandHandler{
    private GahpInterface gahp = null;
    public void setGahp (GahpInterface gahp) {
        this.gahp = gahp;
    }

    public CommandHandlerResponse handleCommand (String[] command) {
        gahp.requestExit();
        return new CommandHandlerResponse (CommandHandlerResponse.SUCCESS, null);
    }
}
