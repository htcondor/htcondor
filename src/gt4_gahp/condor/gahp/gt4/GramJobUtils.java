package condor.gahp.gt4;

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

//import org.globus.exec.client.utils.ManagedJobFactoryHelper;
//import org.globus.exec.client.utils.ManagedJobHelper;
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
import org.globus.exec.utils.ManagedJobFactoryConstants;
import org.globus.exec.service.factory.ManagedJobFactoryHome;
import org.globus.wsrf.utils.AddressingUtils;

import org.globus.exec.utils.rsl.RSLHelper;
import org.globus.exec.utils.rsl.RSLParseException;

import org.globus.security.gridmap.GridMap;

import org.globus.wsrf.ResourceKey;
import org.globus.wsrf.impl.SimpleResourceKey;
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
import org.globus.wsrf.impl.security.descriptor.ClientSecurityDescriptor;
import org.globus.wsrf.impl.security.descriptor.GSISecureMsgAuthMethod;
import org.globus.wsrf.impl.security.descriptor.ResourceSecurityDescriptor;
import org.globus.wsrf.utils.XmlUtils;

import org.apache.axis.message.addressing.AttributedURI;

import org.ietf.jgss.GSSCredential;
import org.ietf.jgss.GSSException;

import org.globus.exec.generated.service.ManagedJobServiceAddressingLocator;
import org.globus.exec.utils.service.ManagedJobHelper;
import org.globus.exec.client.GramJob;

import org.globus.delegation.DelegationConstants;

public class GramJobUtils {


	public static void setDefaultJobAttributes (GramJob gramJob,
												GSSCredential proxy) {
		gramJob.setTimeOut(GramJob.DEFAULT_TIMEOUT);

			// TODO: Change this to HostAuthorization
		gramJob.setAuthorization(HostAuthorization.getInstance());
		gramJob.setMessageProtectionType(Constants.SIGNATURE);

		gramJob.setCredentials (proxy);
	}


	public static void setDefaultJobAttributes (ManagedJobPortType job,
												GSSCredential proxy) {
        setDefaultAttributes( (Stub)job, proxy );
    }

	public static void setDefaultAttributes (Stub port,
                                             GSSCredential proxy) {
        // Set security properties
        ClientSecurityDescriptor secDesc = new ClientSecurityDescriptor();

        secDesc.setGSITransport(Constants.SIGNATURE);

        secDesc.setAuthz(HostAuthorization.getInstance());

        if (proxy != null) {
            secDesc.setGSSCredential(proxy);
        }

        port._setProperty(Constants.CLIENT_DESCRIPTOR,
                          secDesc);
    }

	public static void setDefaultFactoryAttributes (
													ManagedJobFactoryPortType factoryPort,
													GSSCredential proxy) {
			// Set factory security properties

			// TODO: Change this to HostAuthorization
		((Stub)factoryPort)._setProperty (Constants.AUTHORIZATION,
										  HostAuthorization.getInstance());
		((Stub)factoryPort)._setProperty (Constants.GSI_SEC_MSG,
										  Constants.SIGNATURE);
		if (proxy != null ) {
			((Stub)factoryPort)._setProperty (
											  GSIConstants.GSI_CREDENTIALS,
											  proxy);
		}

	}

    public static EndpointReferenceType getEndpoint( String handle ) 
    throws Exception {

        EndpointReferenceType epr = null;

        if ( handle.indexOf( "DelegationService" ) != -1 ) {
            QName delegationKeyQName =
                new QName(DelegationConstants.NS, "DelegationKey");
            int resourceIdStart = handle.indexOf("?") + 1;
            String resourceID = handle.substring(resourceIdStart);
            String serviceAddress = handle.substring(0, resourceIdStart - 1);
            ResourceKey key = new SimpleResourceKey(delegationKeyQName,
                                                    resourceID);
            epr = AddressingUtils.createEndpointReference(serviceAddress, key);
        } else if ( handle.indexOf( "ManagedExecutableJobService" ) != -1 ) {
            epr = ManagedJobHelper.getEndpoint(handle);
        }

        return epr;
    }
}
