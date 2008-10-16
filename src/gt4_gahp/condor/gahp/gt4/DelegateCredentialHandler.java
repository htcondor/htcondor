package condor.gahp.gt4;

import condor.gahp.*;

import java.net.URL;
import java.util.Date;
import java.util.Calendar;

import org.globus.delegation.DelegationUtil;
import org.globus.delegation.DelegationConstants;

import org.apache.axis.message.addressing.EndpointReferenceType;
import org.apache.axis.message.MessageElement;

import org.globus.wsrf.encoding.ObjectSerializer;

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

import org.globus.wsrf.impl.security.authentication.Constants;
import org.globus.wsrf.impl.security.descriptor.ClientSecurityDescriptor;



public class DelegateCredentialHandler implements CommandHandler {
    private GahpInterface gahp;
    
	public void setGahp (GahpInterface g) {
		gahp = g;
	}

    public CommandHandlerResponse handleCommand (String[] cmd) {
        Integer reqId;
        String contactString = null;
        // This is the lifetime of the credential that the delgation
        // service receives
        Date credTermTime = null;

        try {
                // cmd[0] = GT4_DELEGATE_PROXY[_2]
                reqId = new Integer(cmd[1]);
                contactString = cmd[2];
                if ( cmd[0].equals( "GT4_DELEGATE_CREDENTIAL_2" ) ) {
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
			  new DelegateCredentialRunnable(
											 reqId.intValue(),
											 contactString,
											 gahp,
                                             proxy,
                                             credTermTime));
    }

	static class DelegateCredentialRunnable implements Runnable {
        private int requestId;
        private String jobHandle;
        private GahpInterface gahp;
        private GSSCredential proxy;
        private Date credTermTime;

        public DelegateCredentialRunnable (int reqId, String jobHandle, GahpInterface gahp, GSSCredential proxy, Date credTermTime) {
            this.requestId = reqId;
            this.jobHandle = jobHandle;
            this.gahp = gahp;
            this.proxy = proxy;
            this.credTermTime = credTermTime;
        }

        public void run() {
			String delegationEPRString = null;

			try {
				URL url = new URL (jobHandle);
				URL factoryURL=
					new URL (
							 "https",
							 url.getHost(),
							 url.getPort(),
							 DelegationConstants.SERVICE_BASE_PATH + 
							   DelegationConstants.FACTORY_PATH
							 );


			
				ClientSecurityDescriptor desc = new ClientSecurityDescriptor();
				desc.setGSITransport ((Integer)Constants.SIGNATURE);
				 org.globus.axis.util.Util.registerTransport();
				desc.setAuthz(HostAuthorization.getInstance());	
				
				X509Certificate certToSign =
					GSIUtils.getCertificateToSign (factoryURL.toString(),
												   desc);
 
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

		   		EndpointReferenceType delegationEPR = 
					DelegationUtil.delegate(
											   factoryURL.toString(), 
											   globusCred, 
											   certToSign, 
                                               credLifetime,
											   true, 		 // full deleg
											   desc);

				// Convert the Delegation URI to string, and return it
//				QName qName = new QName("", "DelegatedEPR");
//				delegationEPRString = 
//					ObjectSerializer.toString(delegationEPR, 
//										  qName);

                // Convert the Delegation EPR to a URL handle
                QName delegationKeyQName =
                    new QName(DelegationConstants.NS, "DelegationKey");
                String serviceAddress = delegationEPR.getAddress().toString();
                MessageElement referenceProperty =
                    delegationEPR.getProperties().get(delegationKeyQName);
                String resourceID = referenceProperty.getValue(); //key is a string
                String handle = serviceAddress + "?" + resourceID;
                delegationEPRString = serviceAddress + "?" + resourceID;
			} catch (Exception e) {
				e.printStackTrace(System.err);
				String errorString = e.toString();
                if (errorString == null) errorString = "unknown";
				gahp.addResult (
								requestId,
								new String[] {"1", "NULL", errorString});
				return;
			}

			gahp.addResult (
							requestId,
							new String[] {"0", 
									  delegationEPRString, 
									  "NULL"});
		} 
										   
	}
}
 
