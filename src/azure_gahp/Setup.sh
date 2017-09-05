#!/bin/bash
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
install_python_2_7()
{
    sudo yum -y update
    yum groupinstall -y 'development tools'
    yum install -y zlib-dev openssl-devel sqlite-devel bzip2-devel
    yum install xz-libs
    wget http://www.python.org/ftp/python/2.7.10/Python-2.7.10.tar.xz
    xz -d Python-2.7.10.tar.xz
    tar -xvf Python-2.7.10.tar
    cd Python-2.7.10
    ./configure --prefix=/usr/local 
    make
    make altinstall
    ./configure --enable-shared --prefix=/usr/local LDFLAGS="-R /usr/local/lib"
    ls -ltr /usr/bin/python*
    ls -ltr /usr/local/bin/python*
    export PATH="/usr/local/bin:$PATH"
    echo $PATH
}
is_python_version_old()
{
    python_version="$(python -c 'import platform; version=platform.python_version(); print(version)' | grep -Eo '2.[0-9]')"
    echo "Existing version of Python: "
    echo $(python -V)
    echo "Using" $python_version "to compare..."
    if ! command -v python >/dev/null 2>&1
    then
        echo "ERROR: Python not found. 'command -v python' returned failure."
        return 1
    elif [[ $python_version < '2.7' ]]
    then
        return 1
    else
        return 0
    fi
}
install_azure_cli_centos_redhat()
{
    if ! is_python_version_old
    then
        echo "Installing Python 2.7.10..."
        install_python_2_7
    fi
    python -V
    echo "Installing Azure CLI 2.0..."
    #Install azure cli 2.0
    sudo yum -y check-update
    sudo yum install -y gcc libffi-devel python-devel openssl-devel
    # curl -L https://aka.ms/InstallAzureCli > InstallAzureCli.sh
    INSTALL_SCRIPT_URL="https://azurecliprod.blob.core.windows.net/install.py"
    install_script=$(mktemp -t azure_cli_install_tmp_XXXX) || exit
    echo "Downloading Azure CLI install script from $INSTALL_SCRIPT_URL to $install_script."
    curl -# $INSTALL_SCRIPT_URL > $install_script || exit
    # check for the Python installation
    if ! command -v python >/dev/null 2>&1
    then
        echo "ERROR: Python not found. 'command -v python' returned failure."
        echo "If python is available on the system, add it to PATH. For example 'sudo ln -s /usr/bin/python3 /usr/bin/python'"
        exit 1
    fi
    # make python install script executable
    chmod 775 $install_script
    if ! command -v az >/dev/null 2>&1
    then
        echo "Running install script."
        echo $install_script
        if ! is_python_version_old
        then
            yes "" | python2.7 $install_script
        else
            yes "" | python $install_script
        fi
    fi
}
install_azure_cli_ubuntu()
{
    echo "deb [arch=amd64] https://packages.microsoft.com/repos/azure-cli/ wheezy main" | \
    sudo tee /etc/apt/sources.list.d/azure-cli.list
    sudo apt-key adv --keyserver packages.microsoft.com --recv-keys 417A0893
    sudo apt-get install apt-transport-https
    sudo apt-get update && sudo apt-get -y install azure-cli
}
create_cron_job()
{
    # Register cron tab so when machine restart it downloads the secret from azure downloadsecret
    crontab -l > downloadsecretcron
    echo '@reboot /root/scripts/downloadsecret.sh >> /root/scripts/execution.log' >> downloadsecretcron
    crontab downloadsecretcron
    rm downloadsecretcron
}
create_entry_in_initd()
{
    # Create a script in /etc/ini.d/ which will run downloadsecret.sh file at startup
    cat > /etc/init.d/rundownloadsecret << EOF
#!/bin/bash
# chkconfig: 2345 20 80
# description: Execute the shell script at startup which downloads the secret from Azure key vault
/root/scripts/downloadsecret.sh >> /root/scripts/execution.log
exit 0
EOF
    chmod 700 /etc/init.d/rundownloadsecret
    chkconfig --add rundownloadsecret
    chkconfig --level 2345 rundownloadsecret on
    chkconfig --list | grep rundownloadsecret
}
download_secret()
{
    # Execute script
    /root/scripts/downloadsecret.sh >> /root/scripts/execution.log
}
main()
{
    mkdir -p /root/scripts
    touch /root/scripts/downloadsecret.sh
    chmod 700 /root/scripts/downloadsecret.sh
    get_distro_type
    case "$distro_type" in
        "centos" | "redhat")
            install_azure_cli_centos_redhat
            # Retrieve commands which were uploaded from custom data and create shell script
            base64 --decode /var/lib/waagent/CustomData > /root/scripts/downloadsecret.sh
            create_entry_in_initd
            source ~/.bashrc
            download_secret
            ;;
        "ubuntu")
            install_azure_cli_ubuntu
            # Retrieve commands which were uploaded from custom data and create shell script
            cat /var/lib/cloud/instance/user-data.txt > "/root/scripts/downloadsecret.sh"
            create_cron_job
            download_secret
            ;;
    esac
}
main
