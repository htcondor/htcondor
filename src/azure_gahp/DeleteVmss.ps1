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
    $ResourceName = $WebhookBody.VmssName
    $SecureToken = $WebhookBody.SecureToken
    Write-Output ("Runbook started from Webhook " + $WebhookName)
    Write-Output ("Runbook started at "+ $DateTime +"with below parameters - ")
    Write-Output ("Resource group : " + $ResourceGroupName +" & VMSS Name : " + $ResourceName)
}
$token = "sUp3rS3cr3TP@zzwerD"
if($SecureToken -ne $token)
{
    Write-Output "Unauthorized access";
    exit;
}
#Get all ARM resources from all resource groups
$ResourceGroup = Get-AzureRmResourceGroup -name $ResourceGroupName
if($ResourceName -And $ResourceGroup)
{
    $Resource = Get-AzureRmResource -ResourceName $ResourceName -ResourceGroupName $ResourceGroupName
    if($Resource)
    {
        Write-Output ("Deleting vmss: " + $Resource.ResourceName)
        $IsResourceDeleted = Remove-AzureRmResource -ResourceId $Resource.ResourceId -Force
        if($IsResourceDeleted -eq "True")
        {
            Write-Output("Deleted VMSS: " + $Resource.ResourceName)
        }
    }
}
ElseIf($ResourceGroup)
{
    Write-Output ("Deleting Resource Group: " + $ResourceGroupName)
    $IsRgDeleted = Remove-AzureRmResourceGroup -Name $ResourceGroupName -Force
    if($IsRgDeleted -eq "True")
    {
        Write-Output("Deleted Resource Group: " + $ResourceGroupName)
    }
}
Write-Output("Process completed")