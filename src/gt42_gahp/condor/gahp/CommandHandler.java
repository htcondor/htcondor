package condor.gahp;

public interface CommandHandler {
    public void setGahp (GahpInterface gahp);
    public CommandHandlerResponse handleCommand (String[] cmd);
}

