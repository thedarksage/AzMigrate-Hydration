#Provide the subscription Id of the subscription where managed disk exists
sourceSubscriptionId="6d875e77-e412-4d7d-9af4-8895278b4443"
targetSubscriptionId="6d875e77-e412-4d7d-9af4-8895278b4443"

#Provide the name of your resource group where managed disk exists
sourceResourceGroupName="shamish-rg-ccy"

#Provide the name of the managed disk
sourceManagedDiskName="asrseeddisk-negative-negative-3fe29d9e-1b78-475f-9085-2200c49852f1"
targetManagedDiskName="github-gql-negativetest-osDisk00"

#Set the context to the subscription Id where managed disk exists
az account set --subscription $sourceSubscriptionId

#Get the managed disk Id 
managedDiskId=$(az disk show --name $sourceManagedDiskName --resource-group $sourceResourceGroupName --query [id] -o tsv)

#If managedDiskId is blank then it means that managed disk does not exist.
echo 'source managed disk Id is: ' $managedDiskId

#Provide the subscription Id of the subscription where managed disk will be copied to
targetSubscriptionId="6d875e77-e412-4d7d-9af4-8895278b4443"

#Name of the resource group where managed disk will be copied to
targetResourceGroupName="GithubGQL-RG-CCY"

#Set the context to the subscription Id where managed disk will be copied to
az account set --subscription $targetSubscriptionId

#Copy managed disk to different subscription using managed disk Id
az disk create --resource-group $targetResourceGroupName --name $targetManagedDiskName --source $managedDiskId