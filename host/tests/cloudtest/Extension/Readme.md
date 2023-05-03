1. CleanupDeployment.ps1 script removes the older extension deployments from test/stage environment.
2. Uncomment the global variables in CleanupDeployment.ps1, replace with custom values and save it for local run.
3. Trigger CleanupDeployment.ps1 from powershell window by passing the arguments as below
   a. For Stage Environment, refer StageExtCleanupTestJobGroup.xml for the parameters that are passed to CleanupDeployment.ps1
   b. For Test Environment, refer TestExtCleanupTestJobGroup.xml for the parameters that are passed to CleanupDeployment.ps1