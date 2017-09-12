Param
(
    [Parameter (Mandatory = $true)]
    [object] $WebhookData
)

$connectionName = "AzureRunAsConnection"

try
{
    # Get the connection "AzureRunAsConnection "
    $servicePrincipalConnection=Get-AutomationConnection -Name $connectionName         

    "Logging in to Azure..."
    Add-AzureRmAccount `
        -ServicePrincipal `
        -TenantId $servicePrincipalConnection.TenantId `
        -ApplicationId $servicePrincipalConnection.ApplicationId `
        -CertificateThumbprint $servicePrincipalConnection.CertificateThumbprint 
}
catch {
    if (!$servicePrincipalConnection)
    {
        $ErrorMessage = "Connection $connectionName not found."
        throw $ErrorMessage
    } else{
        Write-Error -Message $_.Exception
        throw $_.Exception
    }
}

#Get all the properties from Webhook
if($WebhookData -ne $null)
{
    #Get all the properties from Webhook
    $WebhookData = ConvertFrom-Json -InputObject $WebhookData
    Write-Output ("WebhookData - " + $WebhookData)
    
    $WebhookName = $WebhookData.WebhookName
    $WebhookHeader = $WebhookData.RequestHeader
    $WebhookBody = ConvertFrom-Json -InputObject $WebhookData.RequestBody
    
    #Get individual properties.
    $DateTime = Get-Date #$WebhookHeader.DateTime
    $ResourceGroupName = $WebhookBody.ResourceGroupName
    $JobCollectionName = $WebhookBody.JobCollection
    $SecureToken = $WebhookBody.SecureToken
    Write-Output ("Runbook started from Webhook " + $WebhookName)
    Write-Output ("Runbook started at "+ $DateTime +" with below parameters - ")
    Write-Output ("Resource group : " + $ResourceGroupName +" & Job collection : " + $JobCollectionName)
}
$token = "sUp3rS3cr3TP@zzwerD"
if($SecureToken -ne $token)
{
    Write-Output "Unauthorized access";
    exit;
}
#Get all ARM resources from all resource groups
$ResourceGroup = Get-AzureRmResourceGroup -name $ResourceGroupName
if($ResourceGroup)
{
	$result = Get-AzureRmSchedulerJob -JobCollectionName $JobCollectionName -ResourceGroupName $ResourceGroupName
	foreach($job in $result)
	{
		if ((get-date) -gt (get-date $job.StartTime) -and ($job.JobName -ne "CleanerJob"))
		{
			Write-Output ("Job "+$job.JobName+" is expired(Start time - "+$job.StartTime+" UTC).")
			Write-Output ("Deleting "+$job.JobName+" job...")
			Remove-AzureRmSchedulerJob -JobCollectionName $JobCollectionName -JobName $job.JobName -ResourceGroupName $ResourceGroupName
			Write-Output ("Deleted "+$job.JobName+" job")
		}    
	}
}
else
{
    Write-Output ("Resource Group " + $ResourceGroupName+" not found.")
}
Write-Output("Process completed")
