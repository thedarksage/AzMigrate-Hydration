namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi
{
    public static class CSApiFactory
    {
        public static IProcessServerCSApiStubs GetProcessServerCSApiStubs()
        {
            return new ProcessServerCSApiStubsImpl();
        }

        //public static IMasterTargetCSApiStubs GetMasterTargetCSApiStubs()
        //{
        //    throw new NotImplementedException();
        //}

        //public static ISourceCSApiStubs GetSourceCSApiStubs()
        //{
        //    throw new NotImplementedException();
        //}

        //public static IConfigServerCSApiStubs GetConfigServerCSApiStubs()
        //{
        //    throw new NotImplementedException();
        //}
    }
}
