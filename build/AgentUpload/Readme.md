1. Copy all the files from \\inmstagingsvr\OneOffRequests\AgentUpload to current working directory
2. Copy the files from \build\AgentUpload folder from InMage-Azure-SiteRecovery repo to the current working directory
3. Update current agent version under SupportedPreviousVersions in spv.json
4. Update the global variables in UploadAgent.ps1, replace with custom values and trigger UploadAgent.ps1 the script.