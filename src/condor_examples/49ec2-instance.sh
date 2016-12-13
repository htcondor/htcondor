#/bin/sh

# Acquire the public IP and instance ID of this from the metadata server.
EC2PublicIP=$(/usr/bin/curl -s http://169.254.169.254/latest/meta-data/public-ipv4)
EC2InstanceID=$(/usr/bin/curl -s http://169.254.169.254/latest/meta-data/instance-id)

# If we were installed but for some reason aren't running on EC2, do nothing.
if [ "${EC2InstanceID}"x == x ]; then
	exit 1
fi

# Configure iptables to deny any nonroot user access to the metadata server.
# This will prevent them from using the credentials located there.
/sbin/iptables -A OUTPUT \
	-m owner --destination 169.254.169.254 ! --uid-owner 0 \
	-j REJECT

# Set the EC2PublicIP and EC2InstanceID macros.  The example EC2 configuration
# uses these values (advertises the latter and sets TCP_FORWARDING_HOST to
# the former).
echo "EC2PublicIP = ${EC2PublicIP}"
echo "EC2InstanceID = \"${EC2InstanceID}\""

#
# If we have read permission to a particular file in an S3 bucket,
# and that file is an archive we know how to unzip, download it and
# do so in /etc/condor.  Otherwise, exit non-zero, so that we don't
# force the master to restart pointlessly.
#
function fetchAndExtract() {
	cd /etc/condor
	url=s3://$(echo ${1} | sed -e's/^arn:aws:s3::://')
	if aws s3 cp ${url} . >& /dev/null; then
		file=`basename ${url}`
		echo "# Fetched '${file}' from '${url}'."
		tarfile=`echo ${file} | awk '/.tar.gz$/{print $1}'`
		if [ -n "${tarfile}" ]; then
			tar --no-same-owner -C /etc/condor/config.d -x -z -f ${file}
		fi
		exit 0
	fi
	echo "# Failed to download ${url}..."
}


INSTANCE_PROFILE_ARN=`curl -s http://169.254.169.254/latest/meta-data/iam/info \
	| awk '/InstanceProfileArn/{print $3}' | sed -e's/"//' | sed -e's/",//'`
echo "# Found instance profile ARN '${INSTANCE_PROFILE_ARN}'."

INSTANCE_PROFILE_NAME=`echo ${INSTANCE_PROFILE_ARN} | sed -e's#.*/##'`
echo "# Found instance profile name '${INSTANCE_PROFILE_NAME}'."

INSTANCE_PROFILE_ROLES=`aws iam get-instance-profile --instance-profile-name \
	${INSTANCE_PROFILE_NAME} | awk '/RoleName/{print $2}' |
	sed -e's/"//' | sed -e's/",//'`
echo "# Found instance profile roles: ${INSTANCE_PROFILE_ROLES}"

for role in ${INSTANCE_PROFILE_ROLES}; do
	echo "# Considering instance profile role '${role}'..."

	# Check the inline policies.
	inlinePolicyNames=`aws iam list-role-policies --role-name ${role} \
		| grep -v '[]{}[]' \
		| sed -e's/"//' | sed -e's/",//' | sed -e's/"//'`

	for policy in ${inlinePolicyNames}; do
		echo "# Found inline policy '${policy}'."
		lines=`aws iam get-role-policy --role-name ${role} --policy-name ${policy} \
			| sed -e's/[]{}[]//g' \
			| awk '/^[^*]+$/{print $0}'`
		resourceList=""
		inResourceList=0
		for line in $lines; do
			if [ ${inResourceList} -eq 1 -a ${line} = ',' ]; then
				inResourceList=0
			fi
			if [ ${inResourceList} = 1 ]; then
				line=`echo ${line} | sed -e's/^"//g' | sed -e's/".*$//g'`
				resourceList="${resourceList} ${line}"
			fi
			if [ "${line}" = '"Resource":' ]; then
				inResourceList=1
			fi
		done

		for resource in ${resourceList}; do
			echo "# Found resource: ${resource}"

			# If resource is an S3 ARN and points to a single file, fetch
			# that file and exit.
			arn=`echo ${resource} | awk '/^arn:aws:s3:::/{ print $0 }'`
			if [ "${arn}"x != x ]; then
				fetchAndExtract ${arn}
			fi
		done
	done

	# Check attached policies, too.
	attachedPolicyArns=`aws iam list-attached-role-policies --role-name ${role} \
		| awk '/PolicyArn/{print $2}' \
		| sed -e's/"//' | sed -e's/"//'`

	for policyArn in ${attachedPolicyArns}; do
		echo "# Found attached policy '${policyArn}'."
		version=`aws iam get-policy --policy-arn ${policyArn} \
			| awk '/DefaultVersionId/{print $2}' \
			| sed -e's/"//' | sed -e's/",//'`
		echo -n "# Checking version '${version}'... "
		resource=`aws iam get-policy-version --policy-arn ${policyArn} \
			--version-id ${version} \
			| awk '/Resource/{print $2}' \
			| sed -e's/"//' | sed -e's/",//' \
			| awk '/^[^*]+$/{print $0}'`
		if [ -z "${resource}" ]; then
			echo "found no unwildcarded resources."
			continue
		fi
		echo "# Found resource: ${resource}"

		# If resource is an S3 ARN and points to a single file, fetch
		# that file and exit.
		arn=`echo ${resource} | awk '/^arn:aws:s3:::/{ print $0 }'`
		if [ "${arn}"x != x ]; then
			fetchAndExtract ${arn}
		fi
	done
done

exit 1
