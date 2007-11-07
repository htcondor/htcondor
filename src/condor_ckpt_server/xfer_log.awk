##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

BEGIN {
    num_hosts = 0;
    num_subnets = 0;
    num_users = 0;
}
{
    if ($3 != "RECV" && $3 != "SENT") {
	if (num_hosts == 0) {
	    StartDate = $1;
	    StartTime = $2;
	} else {
	    EndDate = $1;
	    EndTime = $2;
	}
	if (AttemptedSends[$9] == 0 && AttemptedRecvs[$9] == 0) {
	    Hosts[num_hosts] = $9;
	    num_hosts++;
	}
	split($9, ip_tmp, ".");
	Subnet = sprintf("%s.%s.%s", ip_tmp[1], ip_tmp[2], ip_tmp[3]);
	if (AttemptedSends[Subnet] == 0 && AttemptedRecvs[Subnet] == 0) {
	    Subnets[num_subnets] = Subnet;
	    num_subnets++;
	}
	split($10, user_tmp, "\@");
	User = user_tmp[1];
	if (AttemptedSends[User] == 0 && AttemptedRecvs[User] == 0) {
	    Users[num_users] = User;
	    num_users++;
	}
	if ($3 == "S") {
	    AttemptedSends[$9]++;
	    AttemptedSends[Subnet]++;
	    AttemptedSends[User]++;
	    if ($4 != "F") {
		CompletedSends[$9]++;
		CompletedSends[Subnet]++;
		CompletedSends[User]++;
		BytesSent[$9] += $5;
		BytesSent[Subnet] += $5;
		BytesSent[User] += $5;
		TimeSending[$9] += $7;
		TimeSending[Subnet] += $7;
		TimeSending[User] += $7;
	    }
	} else {
	    AttemptedRecvs[$9]++;
	    AttemptedRecvs[Subnet]++;
	    AttemptedRecvs[User]++;
	    if ($4 != "F") {
		CompletedRecvs[$9]++;
		CompletedRecvs[Subnet]++;
		CompletedRecvs[User]++;
		BytesRecvd[$9] += $5;
		BytesRecvd[Subnet] += $5;
		BytesRecvd[User] += $5;
		TimeReceiving[$9] += $7;
		TimeReceiving[Subnet] += $7;
		TimeReceiving[User] += $7;
	    }
	}
    }
}
END {
    qsort(Hosts, 0, num_hosts-1);
    qsort(Subnets, 0, num_subnets-1);
    qsort(Users, 0, num_users-1);
    printf("\nCheckpoint Server Transfer Log Data from %s %s to %s %s\n\n",
	   StartDate, StartTime, EndDate, EndTime);
    printf("%-16.16s %9.9s %7.7s %9.9s %7.7s %9.9s %7.7s %9.9s\n",
	   "", "Send", "Send", "Receive", "Receive", "Total", "Total",
	   "Total");
    printf("%-16.16s %9.9s %7.7s %9.9s %7.7s %9.9s %7.7s %9.9s\n",
	   "", "Bandwidth", "Success", "Bandwidth", "Success", "Bandwidth",
	   "Success", "Traffic");
    printf("%-16.16s %9.9s %7.7s %9.9s %7.7s %9.9s %7.7s %9.9s\n\n",
	   "User/Host/Subnet", "(Mb/s)", "Rate", "(Mb/s)", "Rate", "(Mb/s)",
	   "Rate", "(MB)");
    for (i=0; i < num_users; i++) {
	User = Users[i];
	Bytes = BytesSent[User] + BytesRecvd[User];
	Time = TimeSending[User] + TimeReceiving[User];
	Attempted = AttemptedSends[User] + AttemptedRecvs[User];
	Completed = CompletedSends[User] + CompletedRecvs[User];
	if (TimeSending[User] > 0 && TimeReceiving[User] > 0) {
	    printf("%-16.16s %9.2f %6.0f%% %9.2f %6.0f%% %9.2f %6.0f%% %9.0f\n",
		   User,
		   BytesSent[User]*8/TimeSending[User]/1024/1024,
		   CompletedSends[User]/AttemptedSends[User]*100,
		   BytesRecvd[User]*8/TimeReceiving[User]/1024/1024,
		   CompletedRecvs[User]/AttemptedRecvs[User]*100,
		   Bytes*8/Time/1024/1024,
		   Completed/Attempted*100,
		   (BytesSent[User]+BytesRecvd[User])/1024/1024);
	}
    }
    printf("\n");
    for (i=0; i < num_hosts; i++) {
	Host = Hosts[i];
	Bytes = BytesSent[Host] + BytesRecvd[Host];
	Time = TimeSending[Host] + TimeReceiving[Host];
	Attempted = AttemptedSends[Host] + AttemptedRecvs[Host];
	Completed = CompletedSends[Host] + CompletedRecvs[Host];
	if (TimeSending[Host] > 0 && TimeReceiving[Host] > 0) {
	    printf("%-16.16s %9.2f %6.0f%% %9.2f %6.0f%% %9.2f %6.0f%% %9.0f\n",
		   Host,
		   BytesSent[Host]*8/TimeSending[Host]/1024/1024,
		   CompletedSends[Host]/AttemptedSends[Host]*100,
		   BytesRecvd[Host]*8/TimeReceiving[Host]/1024/1024,
		   CompletedRecvs[Host]/AttemptedRecvs[Host]*100,
		   Bytes*8/Time/1024/1024,
		   Completed/Attempted*100,
		   (BytesSent[Host]+BytesRecvd[Host])/1024/1024);
	}
	TotalBytesSent += BytesSent[Host];
	TotalBytesRecvd += BytesRecvd[Host];
	TotalTimeSending += TimeSending[Host];
	TotalTimeReceiving += TimeReceiving[Host];
	TotalAttemptedSends += AttemptedSends[Host];
	TotalAttemptedRecvs += AttemptedRecvs[Host];
	TotalCompletedSends += CompletedSends[Host];
	TotalCompletedRecvs += CompletedRecvs[Host];
    }
    printf("\n");
    for (i=0; i < num_subnets; i++) {
	Subnet = Subnets[i];
	Bytes = BytesSent[Subnet] + BytesRecvd[Subnet];
	Time = TimeSending[Subnet] + TimeReceiving[Subnet];
	Attempted = AttemptedSends[Subnet] + AttemptedRecvs[Subnet];
	Completed = CompletedSends[Subnet] + CompletedRecvs[Subnet];
	if (TimeSending[Subnet] > 0 && TimeReceiving[Subnet] > 0) {
	    printf("%-16.16s %9.2f %6.0f%% %9.2f %6.0f%% %9.2f %6.0f%% %9.0f\n",
		   Subnet,
		   BytesSent[Subnet]*8/TimeSending[Subnet]/1024/1024,
		   CompletedSends[Subnet]/AttemptedSends[Subnet]*100,
		   BytesRecvd[Subnet]*8/TimeReceiving[Subnet]/1024/1024,
		   CompletedRecvs[Subnet]/AttemptedRecvs[Subnet]*100,
		   Bytes*8/Time/1024/1024,
		   Completed/Attempted*100,
		   (BytesSent[Subnet]+BytesRecvd[Subnet])/1024/1024);
	}
    }
    TotalBytes = TotalBytesSent + TotalBytesRecvd;
    TotalTime = TotalTimeSending + TotalTimeReceiving;
    TotalAttempted = TotalAttemptedSends + TotalAttemptedRecvs;
    TotalCompleted = TotalCompletedSends + TotalCompletedRecvs;
    if (TotalTimeSending > 0 && TotalTimeReceiving > 0) {
	printf("\n%-16.16s %9.2f %6.0f%% %9.2f %6.0f%% %9.2f %6.0f%% %9.0f\n\n",
	       "Overall",
	       TotalBytesSent*8/TotalTimeSending/1024/1024,
	       TotalCompletedSends/TotalAttemptedSends*100,
	       TotalBytesRecvd*8/TotalTimeReceiving/1024/1024,
	       TotalCompletedRecvs/TotalAttemptedRecvs*100,
	       TotalBytes*8/TotalTime/1024/1024,
	       TotalCompleted/TotalAttempted*100,
	       (TotalBytesSent+TotalBytesRecvd)/1024/1024);
    }
    printf("\n%-16.16s %25.25s %25.25s\n\n", "User", "MB Sent", "MB Received");
    for (i=0; i < num_users; i++) {
	User = Users[i];
	printf("%-16.16s %25.0f %25.0f\n",
	       User, BytesSent[User]/1024/1024, BytesRecvd[User]/1024/1024);
    }
    printf("\n%-16.16s %25.0f %25.0f\n",
	   "Total", TotalBytesSent/1024/1024, TotalBytesRecvd/1024/1024);
}
function qsort(v, left, right) {
  if (left >= right) return;
  swap(v, left, int((left + right)/2));
  last = left;
  for (i=left+1; i <= right; i++) {
    if (v[i] < v[left]) swap(v, ++last, i);
  }
  swap(v, left, last);
  qsort(v, left, last-1);
  qsort(v, last+1, right);
}
function swap(v, i, j) {
  temp = v[i];
  v[i] = v[j];
  v[j] = temp;
}
