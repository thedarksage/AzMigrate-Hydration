#ifndef EVTCOLLFORW_RUNNERS_H
#define EVTCOLLFORW_RUNNERS_H

#include "CollForwPair.h"

namespace EvtCollForw
{
    class IRunner : boost::noncopyable
    {
    public:
        virtual ~IRunner() {}

        virtual void Run(boost::function0<bool> cancelRequested) = 0;
    };

    class SerializedRunner : public IRunner
    {
    public:
        SerializedRunner(std::vector<PICollForwPair> pairs)
            : m_collForwPairs(pairs)
        {
            BOOST_ASSERT(!m_collForwPairs.empty());
        }

        virtual ~SerializedRunner() {}

        void Run(boost::function0<bool> cancelRequested)
        {
            int failedPairCreations = 0;
            int failedRuns = 0;

            for (std::vector<PICollForwPair>::iterator itr = m_collForwPairs.begin(); itr != m_collForwPairs.end(); itr++)
            {
                if (cancelRequested && cancelRequested())
                    break;

                // If a pair couldn't be created, continue running other pairs. Finally, throw the failure.
                if (!*itr)
                {
                    failedPairCreations++;
                    continue;
                }

                try
                {
                    while ((*itr)->CollectAndForward(cancelRequested));
                }
                GENERIC_CATCH_LOG_ACTION("Running a pair to completion", failedRuns++);
                // If there's any failure while running a pair to its completion, note that it has
                // failed but proceed to run the remaining pairs. If not, if there's a buggy/unreliable
                // pair, all the remaining pairs would be affected because of it.
            }

            if (failedPairCreations != 0 || failedRuns != 0)
            {
                const char *errMsg = "Running pairs failed in Serialized runner";
                DebugPrintf(EvCF_LOG_ERROR,
                    "%s - %s.  Failed pair creations : %d. Failed runs : %d.\n",
                    FUNCTION_NAME, errMsg, failedPairCreations, failedRuns);
                throw INMAGE_EX(errMsg);
            }

            if (cancelRequested && cancelRequested())
            {
                DebugPrintf(EvCF_LOG_INFO,
                    "%s - Cancellation has been requested during the run.\n", FUNCTION_NAME);
            }
        }

    private:
        std::vector<PICollForwPair> m_collForwPairs;
    };
}

#endif // !EVTCOLLFORW_RUNNERS_H