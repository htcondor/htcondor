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

import org.oasis.wsn.TopicExpressionType;
import org.oasis.wsrf.faults.BaseFaultType;
import org.oasis.wsrf.lifetime.ResourceUnknownFaultType;
import org.oasis.wsrf.properties.ResourcePropertyValueChangeNotificationType;

import java.util.HashMap;

import org.globus.exec.client.GramJob;

public class Gt4GramCallbackAllowHandler implements CommandHandler {
    private GahpInterface gahp;
    public void setGahp (GahpInterface g) {
        gahp = g;
    }

    public CommandHandlerResponse handleCommand (String[] cmd) {
        Integer reqId = null;
        Integer port = null;    
        try {
            reqId = new Integer(cmd[1]);
			if (cmd.length > 2)
				port = new Integer(cmd[2]);
        }  catch (Exception e) {
            e.printStackTrace(System.err);
            return CommandHandlerResponse.SYNTAX_ERROR;
        }

		CallbackSink callbackSink;
		String callbackID;

		try {
			callbackSink = new CallbackSink(reqId.intValue(), port, gahp);
			callbackID = gahp.generateUniqueId();

			CallbackSink.storeCallbackSink (gahp, callbackID, callbackSink);
			gahp.addCleanupStep ((CleanupStep)callbackSink);
		} catch (Exception e) {
            e.printStackTrace(System.err);
            return new CommandHandlerResponse (
                                               CommandHandlerResponse.FAILURE+" 1 "+IOUtils.escapeWord(e.getMessage()));
        }

        return new CommandHandlerResponse (
            CommandHandlerResponse.SUCCESS + " " + callbackID);
    }    


}
 
