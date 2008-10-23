package condor.gahp.gt42;

import condor.gahp.*;

import java.net.URL;
import org.oasis.wsrf.properties.GetResourcePropertyResponse;
import org.globus.exec.utils.client.ManagedJobFactoryClientHelper;
import org.globus.exec.generated.ManagedJobFactoryPortType;
import org.globus.axis.message.addressing.EndpointReferenceType;
import org.globus.exec.utils.ManagedJobFactoryConstants;
import org.ietf.jgss.GSSCredential;


public class Gt42GramPingHandler implements CommandHandler {
    private GahpInterface gahp;
    
	public void setGahp (GahpInterface g) {
		gahp = g;
	}

    public CommandHandlerResponse handleCommand (String[] cmd) {
        Integer reqId;
        String contactString = null;

        try {
                // cmd[0] = GRAM_JOB_REQUEST
                reqId = new Integer(cmd[1]);
                contactString = cmd[2];
        }
        catch (Exception e) {
                e.printStackTrace(System.err);
                return CommandHandlerResponse.SYNTAX_ERROR;
        }

        GSSCredential proxy = GSIUtils.getCredential (gahp);

        MyTask toRun = new MyTask (reqId, 
								   contactString, 
								   gahp,
                                   proxy);

        return new CommandHandlerResponse (
                CommandHandlerResponse.SUCCESS,
                toRun);
    }

    class MyTask implements Runnable {
            
            public final String factoryAddr;
            public final Integer requestID;
            private GahpInterface gahp;
        private GSSCredential proxy;
            
            public MyTask(Integer requestID, 
                            String factoryURL, 
                            GahpInterface gahp,
                          GSSCredential proxy) {
                    this.factoryAddr = factoryURL;
                    this.requestID = requestID;
                    this.gahp = gahp;
                    this.proxy = proxy;
            }
            
            public void run () {
                
                String[] result = new String[] { "0", "NULL" };

                // construct the factory url
                try {
					URL factoryURL = ManagedJobFactoryClientHelper.
                        getServiceURL(this.factoryAddr).getURL();
                    
					EndpointReferenceType factoryEndpoint =
                        ManagedJobFactoryClientHelper.getDefaultFactoryEndpoint(
                        factoryURL.toExternalForm());

					ManagedJobFactoryPortType factory =
						ManagedJobFactoryClientHelper.getPort(factoryEndpoint);					
					GramJobUtils.setDefaultFactoryAttributes (factory,
															  this.proxy);

						// Issue a dummy query to the resource
                        // We don't care about the answer
						// All that really matters is that it doesn't
						// puke up an exception...
					GetResourcePropertyResponse queryResult = 
						factory.getResourceProperty (
													ManagedJobFactoryConstants.RP_DELEGATION_FACTORY_ENDPOINT);

                    if (queryResult == null) {
                        throw new Exception ("Sample query to factory returned null");
                    }
                   
                }
                catch (Exception e) {
                    e.printStackTrace(System.err);
                    String errstr = e.toString();
                    if ( errstr == null || errstr.equals( "" ) ) {
                        errstr = "unknown";
                    }
                    result = new String[] { "1", errstr };
                }
                
                gahp.addResult (requestID.intValue(), result);
            }
    }
}
 
