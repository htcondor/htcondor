package condor.gahp.gt42;

import javax.xml.rpc.Stub;
import javax.xml.namespace.QName;

import org.globus.axis.gsi.GSIConstants;

import org.globus.axis.message.addressing.EndpointReferenceType;

import org.globus.exec.generated.ManagedJobFactoryPortType;
import org.globus.exec.generated.ManagedJobPortType;
import org.globus.wsrf.utils.AddressingUtils;
import org.globus.wsrf.ResourceKey;
import org.globus.wsrf.impl.SimpleResourceKey;
import org.globus.wsrf.impl.security.authentication.Constants;
import org.globus.wsrf.impl.security.authorization.HostAuthorization;
import org.globus.wsrf.impl.security.descriptor.ClientSecurityDescriptor;

import org.ietf.jgss.GSSCredential;

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

        secDesc.setGSISecureTransport(Constants.SIGNATURE);

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
                new QName("http://www.globus.org/08/2004/delegationService", "DelegationKey");
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
