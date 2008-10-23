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

import org.oasis.wsrf.faults.BaseFaultType;

import org.w3c.dom.Element;
import org.w3c.dom.Text;

import org.globus.exec.client.GramJob;


public class Gt4GramJobStatusHandler implements CommandHandler {

    private GahpInterface gahp;

    public void setGahp (GahpInterface gahp) {
        this.gahp = gahp;
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
            new JobStatusRunnable(reqId.intValue(), contactString, gahp,
                                  proxy) );
    } // handleCommand

    class JobStatusRunnable implements Runnable {
        private int requestId;
        private String jobHandle;
        private GahpInterface gahp;
        private GSSCredential proxy;

        public JobStatusRunnable (int reqId, String jobHandle,
                                  GahpInterface gahp, GSSCredential proxy) {
            this.requestId = reqId;
            this.jobHandle = jobHandle;
            this.gahp = gahp;
            this.proxy = proxy;
        }

        public void run() {
            // Job status code
            String statusString = null;
            String faultString = null;
            int exitCode = -1;

            try {

				GramJob gramJob = new GramJob();
				gramJob.setHandle (this.jobHandle);
				
					// Set default attributes
				GramJobUtils.setDefaultJobAttributes (gramJob, this.proxy);

					// Get the state
				gramJob.refreshStatus();
				StateEnumeration jobState = 
					gramJob.getState();

                statusString = jobState.getValue();

                if (jobState.equals (StateEnumeration.Failed)) {
                    BaseFaultType jobFault;
                    jobFault = gramJob.getFault();
                    if ( jobFault != null ) {
                        // TODO should print out full fault text somewhere
                        //   (stderr?)
                        faultString = (jobFault.getDescription()[0]).toString();
                    } else {
                        faultString = "NULL";
                    }
                }

                exitCode = gramJob.getExitCode();

            } catch (Exception e) {
                System.err.println("status failed: ");
                e.printStackTrace(System.err);
                // TODO: Improve this shit
                String errorMessage = (e.getMessage()==null)?"unknown":e.getMessage();
                String [] result = {
                    ""+1, //result code
                    "NULL", // job state
                    "NULL", // job fault string
                    "NULL", // job exit code
                    errorMessage}; // error message
                gahp.addResult (requestId, result); 
                return;
            }

            String[] result = 
                { "0", // result code
                  statusString, // jobState
                  faultString, // job fault string
                  ""+exitCode, // job exit code
                  "NULL" // error message
            };

            gahp.addResult (requestId, result);

        }
    }
} 
