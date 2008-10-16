package condor.gahp.gt4;

import condor.gahp.*;

import java.io.StringReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.math.BigDecimal;
import java.net.MalformedURLException;
import java.net.URL;
import java.rmi.RemoteException;
import java.text.DateFormat;
import java.util.Calendar;
import java.util.HashMap;
import java.util.Vector;
import java.util.List;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.Date;
import java.util.Arrays;

import javax.xml.rpc.ServiceException;
import javax.xml.rpc.Stub;
import javax.xml.namespace.QName;

import org.apache.axis.message.MessageElement;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import org.globus.axis.gsi.GSIConstants;

import org.apache.axis.message.addressing.EndpointReferenceType;


import org.globus.exec.utils.client.ManagedJobFactoryClientHelper;
import org.globus.exec.utils.client.ManagedJobClientHelper;
import org.globus.exec.utils.client.ManagedJobFactoryClientHelper;

import org.globus.exec.generated.CreateManagedJobInputType;
import org.globus.exec.generated.CreateManagedJobOutputType;
import org.globus.exec.generated.FaultType;
import org.globus.exec.generated.FilePairType;
import org.globus.exec.generated.JobDescriptionType;
import org.globus.exec.generated.ManagedJobFactoryPortType;
import org.globus.exec.generated.ManagedJobPortType;
import org.globus.exec.generated.StateChangeNotificationMessageType;
import org.globus.exec.generated.StateEnumeration;
import org.globus.exec.generated.StateEnumeration;
import org.globus.exec.utils.rsl.RSLHelper;
import org.globus.exec.utils.rsl.RSLParseException;

import org.globus.exec.utils.ManagedJobFactoryConstants;
import org.globus.exec.utils.ManagedJobConstants;


import org.globus.security.gridmap.GridMap;

import org.globus.wsrf.WSRFConstants;
import org.globus.wsrf.NotifyCallback;
import org.globus.wsrf.NotificationConsumerManager;
import org.globus.wsrf.NoSuchResourceException;
import org.globus.wsrf.encoding.ObjectDeserializer;
import org.globus.wsrf.impl.security.authentication.Constants;
import org.globus.wsrf.impl.security.authorization.Authorization;
import org.globus.wsrf.impl.security.authorization.HostAuthorization;
import org.globus.wsrf.impl.security.authorization.IdentityAuthorization;
import org.globus.wsrf.impl.security.authorization.SelfAuthorization;
import org.globus.wsrf.impl.security.descriptor.GSISecureMsgAuthMethod;
import org.globus.wsrf.impl.security.descriptor.ResourceSecurityDescriptor;
import org.globus.wsrf.utils.XmlUtils;

import org.ietf.jgss.GSSCredential;
import org.ietf.jgss.GSSException;

import org.oasis.wsrf.properties.GetResourcePropertyResponse;


import org.w3c.dom.Element;
import org.w3c.dom.Text;

public class Gt4GramPingHandler implements CommandHandler {
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
					String factoryType =
						ManagedJobFactoryConstants.DEFAULT_FACTORY_TYPE; 

					URL factoryURL = ManagedJobFactoryClientHelper.
                        getServiceURL(this.factoryAddr).getURL();
                    
					EndpointReferenceType factoryEndpoint =
                        ManagedJobFactoryClientHelper.getFactoryEndpoint(
                        factoryURL,
                        factoryType);

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
 
