#!/bin/bash
if [[ $(id -u) -ne 0 ]] ; then
    echo "$(basename $0) must be run as root"
    exit 1
fi
if [ $# -ne 3 ]; then
    echo "Usage: $(basename $0) <Keyvault Name> <Secret Name> <Tenent Id>"
    exit 1
fi

keyvault_name=$1
secret_name=$2
tenant_id=$3

temp=$(mktemp -d -t download_secret_tmp_XXXX) || exit
echo "Using below temporary location:"
echo $temp

distro_type=""
get_distro_type()
{
    release_info==$(cat /etc/*-release) 
    if [[ $(echo "$release_info" | grep 'Red Hat') != "" ]]; then 
        distro_type="redhat"; 
    elif [[ $(echo "$release_info" | grep 'CentOS') != "" ]] ; then 
        distro_type="centos"; 
    elif [[ $(echo "$release_info" | grep 'Ubuntu') != "" ]] ; then 
        distro_type="ubuntu"; 
    elif [[ $(echo "$release_info" | grep 'Scientfic Linux') != "" ]]; then 
        distro_type="centos";
    fi;
}
get_status_of_last_execution()
{
    if [ $? -eq 0 ]; then
        echo "complete"
    else
        echo "failed. Please see script-execution.log"
        exit 1
    fi
}
# Install jq JSON parser in CentOS or Red Hat
install_jq_redhat_centos()
{
    wget -O jq https://github.com/stedolan/jq/releases/download/jq-1.5/jq-linux64
    chmod +x jq
    cp jq /usr/bin
}
# Install jq JSON parser in Ubuntu
install_jq_ubuntu()
{
    sudo apt-get install jq -y
}
# Install jq JSON parser
install_jq()
{
    case "$distro_type" in
        "centos" | "redhat")
        install_jq_redhat_centos
        ;;
        "ubuntu")
        install_jq_ubuntu
        ;;
    esac
}
# Get access token in JSON format and extract the token value
get_token()
{
    resource="https://vault.azure.net"
    authority="authority=https://login.microsoftonline.com/$tenant_id&resource=$resource"
    localhost_uri="http://localhost:50342/oauth2/token"
    curl -o $temp/token.json --data "$authority" $localhost_uri
    token=$(jq -r '.access_token' $temp/token.json)
}
download_secret()
{
    mkdir -p /root/$keyvault_name
    touch /root/$keyvault_name/$secret_name
    api_version="2016-10-01"
    secret_url="https://$keyvault_name.vault.azure.net/secrets/$secret_name?api-version=$api_version"
    curl -G -H "Authorization: Bearer $token" -o $temp/output.json --url $secret_url
    jq -r 'select(.value != null) | .value' $temp/output.json > /root/$keyvault_name/$secret_name
}
remove_redundant_files()
{
    rm -rf $temp
}

echo "Getting Linux distribution type..."
get_distro_type
echo "Getting Linux distribution type:" $(get_status_of_last_execution)

echo "Checking for jq..."
if ! command -v jq >/dev/null 2>&1
then
    echo "Jq is not installed. Installing jq..."
    install_jq
    echo "Installing jq:" $(get_status_of_last_execution)
else
    echo "jq is already installed"
fi
echo "Checking for jq:" $(get_status_of_last_execution)
jq --version

echo "Getting access token..."
get_token
echo "Getting access token:" $(get_status_of_last_execution)

echo "Downloading secret..."
download_secret
echo "Downloading secret:" $(get_status_of_last_execution)

echo "Deleting redundant files..."
remove_redundant_files
echo "Deleting redundant files:" $(get_status_of_last_execution)
