namespace UnifiedAgentUpgrader
{
    public interface IAgentUpgrade
    {
        /// <summary>
        /// This function is called to upgrade the agent.
        /// It internally performs preactions, upgrade action
        /// and post actions.
        /// </summary>
        /// <returns>True if upgrade succeedes, false otherwise</returns>
        /// <Note> If any other parameter such as exitcode needs to be passed
        /// use PropertyBagDictionary </Note>Note>
        bool RunUpgrade();

        /// <summary>
        /// This functions performs the main upgrade action(s).
        /// </summary>
        /// <returns>0 for success, non 0 for failure</returns>
        int UpgradeAction();

        /// <summary>
        /// This function performs all the preactions
        /// </summary>
        /// <returns>0 for success, non 0 for failure</returns>
        int PreActions();

        /// <summary>
        /// This function performs all the post actions.
        /// </summary>
        /// <returns>0 for success, non 0 for failure</returns>
        int PostActions();

        /// <summary>
        /// This function should be called after post actions
        /// if log upload is required.
        /// </summary>
        /// <returns>0 for success, non 0 for failure</returns>
        int UploadLogs();
    }
}
