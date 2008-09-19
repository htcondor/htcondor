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
import java.util.Iterator;

import javax.xml.rpc.ServiceException;
import javax.xml.rpc.Stub;
import javax.xml.namespace.QName;

import org.apache.axis.message.MessageElement;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import org.globus.axis.gsi.GSIConstants;

import org.apache.axis.message.addressing.EndpointReferenceType;

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
import org.globus.axis.util.Util;

import org.oasis.wsrf.lifetime.WSResourceLifetimeServiceAddressingLocator;
import org.oasis.wsrf.lifetime.ScheduledResourceTermination;

import org.ietf.jgss.GSSCredential;
import org.ietf.jgss.GSSException;

//remove these when no longer needed
import java.io.FileInputStream;
import org.xml.sax.InputSource;


public class Gt4SetTerminationTimeHandler implements CommandHandler {

    // Why is this necessary?
    static {
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
////////////////////
/*
            FileInputStream in = null;
            try {
                in = new FileInputStream(resourceHandle);
                target_epr =
                (EndpointReferenceType)ObjectDeserializer.deserialize(
                    new InputSource(in),
                    EndpointReferenceType.class);
            } finally {
                if (in != null) {
                    try { in.close(); } catch (Exception e) {}
                }
            }
            System.err.println("*** created EPR");
*/
////////////////////
                target_epr = GramJobUtils.getEndpoint( resourceHandle );
                if ( target_epr == null ) {
                    throw new Exception( "unrecognized service type" );
                }

                WSResourceLifetimeServiceAddressingLocator locator =
                    new WSResourceLifetimeServiceAddressingLocator();
//System.err.println("*** created locator");

                ScheduledResourceTermination port = 
                    locator.getScheduledResourceTerminationPort(target_epr);
//System.err.println("*** created port");
////////////////////////
//((Stub)port)._setProperty(GSIConstants.GSI_TRANSPORT,Constants.SIGNATURE);
//((Stub)port)._setProperty (GSIConstants.GSI_CREDENTIALS, proxy);
////////////////////////
                GramJobUtils.setDefaultAttributes( (Stub)port, this.proxy );
//System.err.println("*** set properties");

                org.oasis.wsrf.lifetime.SetTerminationTime request = 
                    new org.oasis.wsrf.lifetime.SetTerminationTime();
//System.err.println("*** created request");
                request.setRequestedTerminationTime(termTime);
//System.err.println("*** called setRequestedTerminationTime");

                org.oasis.wsrf.lifetime.SetTerminationTimeResponse response =
                    port.setTerminationTime(request);
//System.err.println("*** called setTerminationTime");

                Calendar newTermTime = response.getNewTerminationTime();
//System.err.println("*** called getNewTerminationTime");

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
