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


import org.ietf.jgss.GSSCredential;
import org.ietf.jgss.GSSException;


import org.globus.exec.client.GramJob;


public class Gt4GramJobCancelHandler implements CommandHandler {

    private GahpInterface gahp = null;

    public void setGahp (GahpInterface g) {
        gahp = g;
    }
    
    public CommandHandlerResponse handleCommand (String[] cmd) {
        Integer reqId = null;
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

        return new CommandHandlerResponse (
            CommandHandlerResponse.SUCCESS,
            new JobCancelRunnable(reqId.intValue(), contactString, gahp, proxy) );
    } // handleCommand
    

    class JobCancelRunnable implements Runnable {
        private int requestId;
        private String jobHandle;
        private GahpInterface gahp;
        private GSSCredential proxy;

        public JobCancelRunnable (int reqId, String handle, GahpInterface gahp,
                                  GSSCredential proxy) {
            this.requestId = reqId;
            this.jobHandle = handle;
			this.gahp = gahp;
            this.proxy = proxy;
        }
        
        public void run() {
            try {
                // Remove the job listener from callback sinks
                // This has to happen before we cancel the job.
                // Otherwise, the attempt to destroy the subscription
                // source on the server will fail and the server will
                // start to accumulate old junk. So sayeth the Globus
                // people.
                // If we destroy the subscription after the job, then
                // we get a NoSuchResourceException!?!?!?
				List callbackSinkList = 
					CallbackSink.getAllCallbackSinks (this.gahp);
				for (Iterator iter = callbackSinkList.iterator();
					 iter.hasNext(); ) {
					((CallbackSink)iter.next()).removeJobListener (
							   this.jobHandle);
				}

				GramJob gramJob = new GramJob();
				gramJob.setHandle (this.jobHandle);
				
					// Set default attributes
				GramJobUtils.setDefaultJobAttributes (gramJob, this.proxy);

                // If we don't set this, gramJob.cancel() will destroy the
                // delegated credential, which is shared with other jobs
                gramJob.setDelegationEnabled( false );

					// Cancel the job
				gramJob.cancel();

            } catch (Exception e) {
                System.err.println("cancel failed: ");
                e.printStackTrace(System.err);
                
                String errorMessage = 
					(e.getMessage()==null)?"unknown":e.getMessage();
                String [] result = {
                    ""+1, //result code
                    errorMessage}; // job state

                gahp.addResult (requestId, result);
				return;
            }
            
            String[] result = { "0", "NULL" };
            gahp.addResult (requestId, result);

        }
    }
} // GRAM_JOB_CANCEL_Handler
