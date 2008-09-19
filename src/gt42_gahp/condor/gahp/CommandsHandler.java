package condor.gahp;

public class CommandsHandler implements CommandHandler{
    protected GahpInterface gahp;
    public void setGahp (GahpInterface g) {
        this.gahp = g;
    }

    public CommandHandlerResponse handleCommand (String[] cmd) {
        if (!(cmd.length == 1 && cmd[0].equals("COMMANDS"))) {
            return CommandHandlerResponse.SYNTAX_ERROR;
        }

        String [] commands = gahp.getCommands();
                
        StringBuffer buff = new StringBuffer().append (CommandHandlerResponse.SUCCESS);

        for (int i=0; i<commands.length; i++) {
            buff.append (' ').append (commands[i]);
        }

        return new CommandHandlerResponse (
                                           (String)buff.toString(),
                                           (Runnable)null);
    }
}
