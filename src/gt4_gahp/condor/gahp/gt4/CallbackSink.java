/*
 * CallbackSink.java
 *
 * Created on October 7, 2003, 4:01 PM
 */

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
import org.globus.exec.utils.ManagedJobConstants;
import org.globus.exec.utils.ManagedJobFactoryConstants;
import org.globus.exec.utils.rsl.RSLHelper;
import org.globus.exec.utils.rsl.RSLParseException;
import org.globus.exec.utils.FaultUtils;

import org.globus.security.gridmap.GridMap;

import org.globus.wsrf.WSRFConstants;
import org.globus.wsrf.WSNConstants;

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
import org.globus.wsrf.impl.security.descriptor.GSITransportAuthMethod;
import org.globus.wsrf.impl.security.descriptor.ResourceSecurityDescriptor;
import org.globus.wsrf.utils.XmlUtils;
import org.globus.exec.generated.FaultResourcePropertyType;

import org.ietf.jgss.GSSCredential;
import org.ietf.jgss.GSSException;

import org.oasis.wsn.TopicExpressionType;
import org.oasis.wsrf.faults.BaseFaultType;
import org.oasis.wsrf.lifetime.ResourceUnknownFaultType;
import org.oasis.wsrf.properties.ResourcePropertyValueChangeNotificationType;
import org.oasis.wsrf.lifetime.Destroy;

import org.w3c.dom.Element;
import org.w3c.dom.Text;

import org.globus.exec.client.GramJob;
import org.globus.wsrf.container.ServiceContainer;
import org.globus.exec.utils.client.ManagedJobClientHelper;

import java.util.Map;
import java.util.HashMap;


import org.apache.axis.message.MessageElement;
import org.apache.axis.message.addressing.AttributedURI;
import org.apache.axis.message.addressing.EndpointReferenceType;

import org.oasis.wsn.SubscriptionManager;
import org.oasis.wsn.TopicExpressionType;
import org.oasis.wsn.WSBaseNotificationServiceAddressingLocator;
import org.oasis.wsn.Subscribe;
import org.oasis.wsn.SubscribeResponse;


/**
 *
 * @author  ckireyev
 *
 * This class is a wrapper around NotifyCallback - 
 * the GT4 way of receiving callbacks
 *
 */
public class CallbackSink
    implements CleanupStep, NotifyCallback
{
	

    private String callbackId;
    private int requestId;
    private HashMap notificationProducers  = new HashMap();
    private GahpInterface gahp;
    private boolean isInitialized = false;
	private NotificationConsumerManager notificationConsumerManager;
	private Integer port = null;
    private EndpointReferenceType notificationConsumerEPR = null;
    
    public static final String CALLBACK_SINK = "CALLBACK_SINK";
	public static final String CALLBACK_SINK_LIST = "CALLBACK_SINK_LIST";
    
    /** Creates a new instance of CallbackSink */
    public CallbackSink (int requestId,
						 Integer port,
						 GahpInterface gahp) 
		throws Exception {
		this.requestId = requestId;
		this.gahp = gahp;
		this.port = port;

			// Start listening on a specified port...
		Map props = new HashMap();
		if (this.port != null) {
			props.put (ServiceContainer.PORT, 
					   this.port);
		}

        //make sure the embedded container speaks GSI Transport Security
        props.put(ServiceContainer.CLASS,
                  "org.globus.wsrf.container.GSIServiceContainer");

        // TODO Should we insert the currently active proxy in the
        //   properties? If we don't, does that mean no authentication
        //   is occurring? If we do, I think we need to refresh the
        //   credential when our invoker gives us a fresh one.

		this.notificationConsumerManager = 
			NotificationConsumerManager.getInstance(props);
		this.notificationConsumerManager.startListening();

			// Create a security descriptor
		ResourceSecurityDescriptor securityDescriptor	
                = new ResourceSecurityDescriptor();
		securityDescriptor.setAuthz(Authorization.AUTHZ_NONE);
		Vector authMethod = new Vector();
        authMethod.add(GSITransportAuthMethod.BOTH);
		securityDescriptor.setAuthMethods(authMethod);

		List topicPath = new LinkedList(); 	
		topicPath.add(ManagedJobConstants.RP_STATE);
		notificationConsumerEPR = 
			notificationConsumerManager.createNotificationConsumer(
															topicPath,
															this,
															securityDescriptor);

		isInitialized = true;
	}

	public void addJobListener (GramJob job, String jobId) 
		throws Exception 
	{
		ManagedJobPortType jobPort = 
			ManagedJobClientHelper.getPort(job.getEndpoint());

		Subscribe request = new Subscribe();
		request.setConsumerReference(notificationConsumerEPR);
		TopicExpressionType topicExpression = 
			new TopicExpressionType(
									WSNConstants.SIMPLE_TOPIC_DIALECT,
									ManagedJobConstants.RP_STATE);
		request.setTopicExpression(topicExpression);	

		GramJobUtils.setDefaultJobAttributes (jobPort, 
											  GSIUtils.getCredential (gahp));
		SubscribeResponse response = jobPort.subscribe (request);

        EndpointReferenceType notificationProducerEPR = 
            response.getSubscriptionReference();

        synchronized( notificationProducers ) {
            notificationProducers.put (jobId, notificationProducerEPR);
        }
	}


	public void removeJobListener (String jobContact) {
        EndpointReferenceType notificationProducerEPR;

        synchronized( notificationProducers ) {
            notificationProducerEPR = (EndpointReferenceType)notificationProducers.remove (jobContact);
        }

        if ( notificationProducerEPR == null ) {
            // We don't have a notification EPR for this job contact.
            // There's no work to do, so return immediately.
            return;
        }

        // Now we destroy the notification producer on the server
        try {
            SubscriptionManager subscriptionPort
                = new WSBaseNotificationServiceAddressingLocator().
                getSubscriptionManagerPort(notificationProducerEPR);

            GramJobUtils.setDefaultAttributes((Stub)subscriptionPort,
                                              GSIUtils.getCredential(gahp));

            subscriptionPort.destroy(new Destroy());
        }
        catch ( Exception e ) {
        }
	}

    
    public synchronized void doCleanup() {
        try {
            this.notificationConsumerManager.removeNotificationConsumer(
                                                    notificationConsumerEPR);

            this.notificationConsumerManager.stopListening();

            // TODO should we try to destroy all of the subscription
            //   objects on the servers here?
			notificationProducers.clear();
        }
        catch (Exception e) {
            e.printStackTrace(System.err);
        }
    }

    
    public void deliver (List topicPath,
						 EndpointReferenceType producer,
						 Object message)
    {
        String jobContact = null;
		String jobState = null;
        int jobExitCode = -1;
		BaseFaultType jobFault = null;

		try {
            jobContact = ManagedJobClientHelper.getHandle( producer );

			StateChangeNotificationMessageType changeNotification =
                (StateChangeNotificationMessageType)ObjectDeserializer.toObject(
                    (Element) message,
                    StateChangeNotificationMessageType.class);
            StateEnumeration state = changeNotification.getState();
            jobState = state.getValue();
			if (state.equals (StateEnumeration.Failed)) {
				jobFault = getFaultFromRP(changeNotification.getFault());
			}
            jobExitCode = changeNotification.getExitCode();
        }
        catch (Exception e) {
            // TODO Print out the exception
            System.err.println( "Exception while processing callback" );
            return;
		}

        if ( jobFault != null ) {
            System.err.println( "Full fault for job " + jobContact + ":" );
            System.err.println( FaultUtils.faultToString( jobFault ) );
        }

		gahp.addResult (
						requestId,
						new String[] {
							jobContact,
							jobState,
							(jobFault!=null)?(jobFault.getDescription()[0]).toString():"NULL",
                            (jobExitCode==-1) ? "NULL" : ""+jobExitCode
                           }
						);

    }

		// This is copied from GramJob.java
    private BaseFaultType getFaultFromRP(FaultResourcePropertyType fault)
    {
        if (fault == null)
        {
            return null;
        }

        if (fault.getFault() != null) {
            return fault.getFault();
        } else if (fault.getCredentialSerializationFault() != null) {
            return fault.getCredentialSerializationFault();
        } else if (fault.getExecutionFailedFault() != null) {
            return fault.getExecutionFailedFault();
        } else if (fault.getFilePermissionsFault() != null) {
            return fault.getFilePermissionsFault();
        } else if (fault.getInsufficientCredentialsFault() != null) {
            return fault.getInsufficientCredentialsFault();
         } else if (fault.getInternalFault() != null) {
            return fault.getInternalFault();
        } else if (fault.getInvalidCredentialsFault() != null) {
            return fault.getInvalidCredentialsFault();
        } else if (fault.getInvalidPathFault() != null) {
            return fault.getInvalidPathFault();
        } else if (fault.getServiceLevelAgreementFault() != null) {
            return fault.getServiceLevelAgreementFault();
        } else if (fault.getStagingFault() != null) {
            return fault.getStagingFault();
        } else if (fault.getUnsupportedFeatureFault() != null) {
            return fault.getUnsupportedFeatureFault();
        } else {
            return null;
        }
    }

    public static void storeCallbackSink (GahpInterface gahp, 
										  String id, 
										  CallbackSink sink) {
        gahp.storeObject (CALLBACK_SINK+id, sink);

			// And add it to the lsit
		List sinkList = getAllCallbackSinks(gahp);
		sinkList.add (sink);
    }
    

    public static CallbackSink getCallbackSink (GahpInterface gahp, 
												String id) {
        return (CallbackSink)gahp.getObject (CALLBACK_SINK+id);
    }


	public static List getAllCallbackSinks (GahpInterface gahp) {
		List list = null;
		synchronized (gahp) {
			list = (List)gahp.getObject (CALLBACK_SINK_LIST);
			if (list == null) {
				list = new Vector();
				gahp.storeObject (CALLBACK_SINK_LIST, list);
			}
		}

		return list;
	}

}
