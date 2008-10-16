package condor.gahp.gt4;

import condor.gahp.*;

import org.ietf.jgss.GSSCredential;
import org.ietf.jgss.GSSException;
import org.gridforum.jgss.ExtendedGSSManager;
import org.gridforum.jgss.ExtendedGSSCredential;

import java.io.*;

import org.apache.axis.message.addressing.EndpointReferenceType;
import java.security.cert.X509Certificate;

import org.globus.delegation.DelegationUtil;
import org.globus.wsrf.utils.AddressingUtils;

import org.globus.wsrf.impl.security.authentication.Constants;
import org.globus.wsrf.impl.security.descriptor.ClientSecurityDescriptor;

import org.globus.gsi.GlobusCredential;
import org.globus.gsi.gssapi.GlobusGSSCredentialImpl;

/**
 *
 * @author  ckireyev
 */
public abstract class GSIUtils {
    public static final String DEFAULT = "DEFAULT";
    private static final String CURRENT_GSI_CREDENTIAL = "CURRENT_GSI_CREDENTIAL";
    public static final String GSI_CREDENTIAL = "GSI_CREDENTIAL : ";
    
    public static GSSCredential getCredential (GahpInterface gahp) {
        GSSCredential cred = getCurrentCredential(gahp);
        
        // If no current, get default
        if (cred == null) {
            cred = getDefaultCredential (gahp);
        }
        
        return cred;
    }
    
    public static GSSCredential getDefaultCredential (GahpInterface gahp) {
        GSSCredential cred = null; 
        cred = (GSSCredential)gahp.getObject(GSI_CREDENTIAL+DEFAULT);
        return cred;
    }
    
    public static GSSCredential getCurrentCredential (GahpInterface gahp) {
        GSSCredential cred = null; 
        cred = (GSSCredential)gahp.getObject(CURRENT_GSI_CREDENTIAL);
        return cred;
    }
    
    public static void setDefaultCredential (GahpInterface gahp, GSSCredential cred) {
        GlobusCredential.setDefaultCredential( ((GlobusGSSCredentialImpl)cred).getGlobusCredential() );
        gahp.storeObject (GSI_CREDENTIAL+DEFAULT, cred);
    }
    
    public static void cacheCredential (GahpInterface gahp, String key, GSSCredential cred) {
        gahp.storeObject (GSI_CREDENTIAL+key, cred);
    }
    
    public static void setCurrentCredential(GahpInterface gahp, String key) throws Exception {
        GSSCredential cred = (GSSCredential)gahp.getObject (GSI_CREDENTIAL+key);
        if (cred == null) {
            throw new Exception ("Non-existent credential id "+key);
        }
        gahp.storeObject (CURRENT_GSI_CREDENTIAL, cred);
    }
    
    public static void deleteCredential(GahpInterface gahp, String key) throws Exception {
        GSSCredential cred = (GSSCredential)gahp.getObject (GSI_CREDENTIAL+key);
        if (cred == null) {
            throw new Exception ("Invalid credential id "+key);
        }
        gahp.removeObject (key);
        
        // If this credential is the current credential clear the current credential
        if (cred == getCurrentCredential(gahp))
            gahp.removeObject (CURRENT_GSI_CREDENTIAL);
    }
    
    public static GSSCredential readCredentialFromFile (String fileName) throws IOException, GSSException {
        byte [] data = new byte[4096];
        File file = new File (fileName);
        if (!file.exists()) {
            throw new IOException ("File "+fileName+" does not exist");
        }
        FileInputStream in = new FileInputStream(file.getAbsolutePath());
        ByteArrayOutputStream outStream = new ByteArrayOutputStream();
        int len = 0;
        do {
            len = in.read(data);
            if (len > 0) 
                outStream.write(data, 0, len);
        } while (len > 0);
        in.close();

        ExtendedGSSManager manager = (ExtendedGSSManager)
            ExtendedGSSManager.getInstance();
        GSSCredential cred = manager.createCredential(outStream.toByteArray(),
                ExtendedGSSCredential.IMPEXP_OPAQUE,
                GSSCredential.DEFAULT_LIFETIME,null,
                GSSCredential.INITIATE_AND_ACCEPT);

        return cred;
    }

	public static X509Certificate getCertificateToSign(String factoryUrl, ClientSecurityDescriptor secDescriptor) throws Exception {

			// Get the public key to delegate on.
		EndpointReferenceType delegEpr = 
			AddressingUtils.createEndpointReference(factoryUrl, null);
		X509Certificate[] certsToDelegateOn =
			DelegationUtil.getCertificateChainRP(delegEpr, secDescriptor);    
		return certsToDelegateOn[0];
	}

}
