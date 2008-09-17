package condor.gahp;

import java.util.Vector;
import java.util.List;

public class CommandResultQueue {
    private Vector resultQueue = new Vector();

    public CommandResultQueue() {
    }

    public int size() {
        return this.resultQueue.size();
    }

    public synchronized boolean addResult(int reqId, String [] result) {
        resultQueue.add (new CommandResult (reqId, result));
        return true;
    }

    public synchronized CommandResult[] flushResultList () {
        CommandResult[] result = new CommandResult[resultQueue.size()];
        resultQueue.copyInto (result);

        resultQueue.clear();
        return result;
    }

    public String toString() {
        return resultQueue.toString();
    }

    class CommandResult {
        public final int requestID;
        public final String [] result;

        public CommandResult(int reqId, String [] result) {
            this.requestID=reqId;
            this.result=result;
        }

        public String toString() {
            return requestID+ " " + result;
        }

    }
}
