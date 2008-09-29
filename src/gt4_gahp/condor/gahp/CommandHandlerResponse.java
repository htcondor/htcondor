package condor.gahp;

public class CommandHandlerResponse {
    public static final String SUCCESS = "S";
    public static final String ERROR = "E";
    public static final String FAILURE = "F";

    public final String[] returnLine;
    public final Runnable toRun;

    public CommandHandlerResponse (String a) {
        this (a, null);
    }
    public CommandHandlerResponse (String a,
                                   Runnable b) {
        this (new String[] {a }, b);
    }
         
    // This should be used by commands with more than one return line
    // At this moment i can only think of "RESULTS"
    public CommandHandlerResponse (String [] a, Runnable b) {
        returnLine=a;
        toRun=b;
    }

    public static final CommandHandlerResponse SYNTAX_ERROR = 
        new CommandHandlerResponse (ERROR, null);
}
