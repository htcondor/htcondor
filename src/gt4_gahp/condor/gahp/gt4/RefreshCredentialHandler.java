package condor.gahp.gt4;

import condor.gahp.*;

import java.net.URL;
import java.util.Date;
import java.util.Calendar;

import org.globus.delegation.DelegationUtil;
import org.globus.delegation.DelegationConstants;

import org.apache.axis.message.addressing.EndpointReferenceType;

import org.globus.wsrf.encoding.ObjectDeserializer;

import org.globus.wsrf.impl.security.util.AuthUtil;

import org.globus.wsrf.impl.security.authorization.Authorization;
import org.globus.wsrf.impl.security.authorization.HostAuthorization;

import org.xml.sax.InputSource;

import java.io.FileInputStream;

import java.security.cert.X509Certificate;

import org.globus.wsrf.utils.AddressingUtils;

import org.globus.gsi.GlobusCredential;

import javax.xml.namespace.QName;

import org.ietf.jgss.GSSCredential;

import org.globus.gsi.gssapi.GlobusGSSCredentialImpl;

import java.io.ByteArrayInputStream;

import org.globus.wsrf.impl.security.authentication.Constants;
import org.globus.wsrf.impl.security.descriptor.ClientSecurityDescriptor;

import org.globus.wsrf.ResourceKey;
import org.globus.wsrf.impl.SimpleResourceKey;

import org.globus.axis.util.Util;

public  class RefreshCredentialHandler implements CommandHandler {
    private GahpInterface gahp;
    
	public void setGahp (GahpInterface g) {
		gahp = g;
	}

    public CommandHandlerResponse handleCommand (String[] cmd) {
        Integer reqId;
        String delegURI = null;
        // This is the lifetime of the credential that the delgation
        // service receives
        Date credTermTime = null;

        try {
                // cmd[0] = GT4_REFRESH_CREDENTIAL[_2]
                reqId = new Integer(cmd[1]);
                delegURI = cmd[2];
                if ( cmd[0].equals( "GT4_REFRESH_CREDENTIAL_2" ) ) {
                    long term_time = Long.parseLong( cmd[3] );
                    if ( term_time > 0 ) {
                        credTermTime = new Date( term_time * 1000 );
                    }
                } else {
                    Calendar calTermTime = Calendar.getInstance();
                    calTermTime.add( Calendar.HOUR, 12 );
                    credTermTime = calTermTime.getTime();
                }
        }
        catch (Exception e) {
                e.printStackTrace(System.err);
                return CommandHandlerResponse.SYNTAX_ERROR;
        }

        GSSCredential proxy = GSIUtils.getCredential (gahp);
          
		return new CommandHandlerResponse (
			   CommandHandlerResponse.SUCCESS,
			   new RefreshCredentialRunnable (reqId.intValue(),
											  delegURI, 
											  gahp,
                                              proxy,
                                              credTermTime));
    }

	static class RefreshCredentialRunnable implements Runnable {
        private int requestId;
        private String delegationURI;
        private GahpInterface gahp;
        private GSSCredential proxy;
        private Date credTermTime;

        public RefreshCredentialRunnable (int reqId, String uri, GahpInterface gahp, GSSCredential proxy, Date credTermTime) {
            this.requestId = reqId;
            this.delegationURI = uri;
            this.gahp = gahp;
            this.proxy = proxy;
            this.credTermTime = credTermTime;
        }

        public void run() {
			try {
                // Reconstitute the EPR from the handle
                QName delegationKeyQName =
                    new QName(DelegationConstants.NS, "DelegationKey");
                int resourceIdStart = delegationURI.indexOf("?") + 1;
                String resourceID = delegationURI.substring(resourceIdStart);
                String serviceAddress =
                    delegationURI.substring(0, resourceIdStart - 1);
                ResourceKey key = new SimpleResourceKey(
                                        delegationKeyQName, resourceID);
                EndpointReferenceType delegationEPR = AddressingUtils.createEndpointReference(serviceAddress, key);

//				InputSource in =
//					new InputSource (
//					    new ByteArrayInputStream (
//							delegationURI.getBytes()));

				
//				EndpointReferenceType delegationEPR = 
//				   (EndpointReferenceType)ObjectDeserializer
//					.deserialize(in, 
//								 EndpointReferenceType.class);

			
				String factoryURL=
					new URL (
							 delegationEPR.getAddress().getScheme(),
							 delegationEPR.getAddress().getHost(),
							 delegationEPR.getAddress().getPort(),
							 DelegationConstants.SERVICE_BASE_PATH + 
							 DelegationConstants.FACTORY_PATH)
					.toString();

				ClientSecurityDescriptor desc = new ClientSecurityDescriptor();
                desc.setGSITransport((Integer)Constants.SIGNATURE);
                Util.registerTransport();
				desc.setAuthz(HostAuthorization.getInstance());	
		

				X509Certificate certToSign =
					GSIUtils.getCertificateToSign (factoryURL, desc);

				GlobusCredential globusCred =
					((GlobusGSSCredentialImpl)this.proxy).getGlobusCredential();

                int credLifetime;
                if ( credTermTime == null ) {
                    credLifetime =
                        (new Long(globusCred.getTimeLeft())).intValue();
                } else {
                    long lifetime = ( credTermTime.getTime() -
                                      (new Date()).getTime()
                                    ) / 1000;
                    credLifetime = (new Long( lifetime )).intValue();
                }

				DelegationUtil.refresh(globusCred,
									   certToSign, 
                                       credLifetime,
									   true,
									   desc,
									   delegationEPR );
			}
			catch (Exception e) {
				e.printStackTrace(System.err);
				String errorString = e.toString();
                if (errorString == null) errorString = "unknown";
				gahp.addResult (
								requestId,
								new String[] {"1", errorString});
				return;
			} //yrt

			gahp.addResult (
							requestId,
							new String[] {"0", "NULL"});
		} // method
	}
}

 
