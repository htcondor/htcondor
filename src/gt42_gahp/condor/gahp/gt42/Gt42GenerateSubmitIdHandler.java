package condor.gahp.gt42;

import condor.gahp.*;

import org.apache.axis.components.uuid.UUIDGen;
import org.apache.axis.components.uuid.UUIDGenFactory;


public class Gt42GenerateSubmitIdHandler implements CommandHandler {
    private GahpInterface gahp;
    private static UUIDGen uuidgen = null;
    
    public void setGahp (GahpInterface g) {
        gahp = g;
    }

    public CommandHandlerResponse handleCommand (String[] cmd) {
        Integer reqId;

        try {
            reqId = new Integer(cmd[1]);
		}
        catch (Exception e) {
            e.printStackTrace(System.err);
            return CommandHandlerResponse.SYNTAX_ERROR;
        }

        // Multiple calls to UUIDGenFactory.getUUIDGen() return UUIDGen
        // objects that generate identical UUIDs. Therefore, we create
        // one UUIDGen and keep it around for all our UUID needs.
        if ( uuidgen == null ) {
            uuidgen = UUIDGenFactory.getUUIDGen();
        }

        GenerateId toRun = new GenerateId( uuidgen, reqId, gahp );

        return new CommandHandlerResponse( CommandHandlerResponse.SUCCESS,
                                           toRun );
    }

    class GenerateId implements Runnable {
        public final Integer requestID;
        private GahpInterface gahp;
        private UUIDGen uuidgen;

        public GenerateId( UUIDGen uuidgen,
                           Integer requestID,
                           GahpInterface gahp )
        {
            this.uuidgen = uuidgen;
            this.requestID = requestID;
            this.gahp = gahp;
        }

        public void run()
        {
            // Generate unique Id
            String submissionID = "uuid:"+uuidgen.nextUUID();

            gahp.addResult( requestID.intValue(),
                            new String [] { submissionID } );
        }
    }

} // class GENERATE_SUBMIT_ID handler
