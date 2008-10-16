package condor.gahp.gt42;

import condor.gahp.*;

import java.util.Calendar;
import java.net.URL;
import javax.xml.rpc.Stub;
import org.oasis.wsrf.lifetime.ScheduledResourceTermination;
import org.oasis.wsrf.lifetime.WSResourceLifetimeServiceAddressingLocator;
import org.globus.axis.message.addressing.EndpointReferenceType;
import org.ietf.jgss.GSSCredential;
import org.globus.axis.util.Util;


public class Gt42SetTerminationTimeHandler implements CommandHandler {

    // Util.registerTransport() needs to be called once before any
    // communications. This is a poor place to put it, but that's where
    // it ended up.
    // If Util.registerTransport() is called before any URL objects of
    // type https are instantiated, URL.getPort() starts returning 443
    // for https URLs that didn't specify a port when it should return -1.
    // This screws up ServiceURL, which tries to provide a default port
    // of 8443.
    static {
        try {
            URL url = new URL("https://foo");
        } catch ( Exception e ) {
        }
        Util.registerTransport();
    }

    private GahpInterface gahp = null;

    public void setGahp (GahpInterface g) {
        gahp = g;
    }
    
    public CommandHandlerResponse handleCommand (String[] cmd) {
        Integer reqId = null;
        String handle = null;
        Calendar termTime = null;
        Long longTermTime = null;

        try {
            // cmd[0] = GT4_SET_TERMINATION_TIME[_2]
            reqId = new Integer(cmd[1]);
            handle = cmd[2];
            longTermTime = new Long(cmd[3]);
            termTime = Calendar.getInstance();
            if ( cmd[0].equals( "GT4_SET_TERMINATION_TIME" ) ) {
                termTime.add( Calendar.SECOND, longTermTime.intValue() );
            } else {
                termTime.setTimeInMillis( longTermTime.longValue() * 1000 );
            }
        }
        catch (Exception e) {
            e.printStackTrace(System.err);
            return CommandHandlerResponse.SYNTAX_ERROR;
        }
        
        GSSCredential proxy = GSIUtils.getCredential (gahp);

        return new CommandHandlerResponse (
            CommandHandlerResponse.SUCCESS,
            new SetTerminationTimeRunnable(reqId.intValue(), handle,
                                           termTime, gahp, proxy) );
    } // handleCommand
    

    class SetTerminationTimeRunnable implements Runnable {
        private int requestId;
        private String resourceHandle;
        private Calendar termTime;
        private GahpInterface gahp;
        private GSSCredential proxy;

        public SetTerminationTimeRunnable (int reqId, String handle,
                                           Calendar termTime,
                                           GahpInterface gahp,
                                           GSSCredential proxy) {
            this.requestId = reqId;
            this.resourceHandle = handle;
            this.termTime = termTime;
			this.gahp = gahp;
            this.proxy = proxy;
        }
        
        public void run() {

            long utcNewTermTime = 0;

            try {

                EndpointReferenceType target_epr = null;

                // Produce EPR
                target_epr = GramJobUtils.getEndpoint( resourceHandle );
                if ( target_epr == null ) {
                    throw new Exception( "unrecognized service type" );
                }

                WSResourceLifetimeServiceAddressingLocator locator =
                    new WSResourceLifetimeServiceAddressingLocator();

                ScheduledResourceTermination port = 
                    locator.getScheduledResourceTerminationPort(target_epr);
                GramJobUtils.setDefaultAttributes( (Stub)port, this.proxy );

                org.oasis.wsrf.lifetime.SetTerminationTime request = 
                    new org.oasis.wsrf.lifetime.SetTerminationTime();
                request.setRequestedTerminationTime(termTime);

                org.oasis.wsrf.lifetime.SetTerminationTimeResponse response =
                    port.setTerminationTime(request);

                Calendar newTermTime = response.getNewTerminationTime();

                utcNewTermTime = newTermTime.getTime().getTime() / 1000;

            } catch (Exception e) {
                System.err.println("SetTerminationTime failed: ");
                e.printStackTrace(System.err);
                
                String errorMessage = 
					(e.getMessage()==null)?"unknown":e.getMessage();
                String [] result = {
                    ""+1, //result code
                    ""+0,
                    errorMessage}; // error message

                gahp.addResult (requestId, result);
				return;
            }

            String[] result = { "0", ""+utcNewTermTime, "NULL" };
            gahp.addResult (requestId, result);

        }
    }
} // Gt4SetTerminationTimeHandler
