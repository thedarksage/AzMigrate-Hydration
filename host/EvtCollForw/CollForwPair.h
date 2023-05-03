#ifndef EVTCOLLFORW_PAIR_H
#define EVTCOLLFORW_PAIR_H

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>

#include "Collectors.h"
#include "Converters.h"
#include "EvtCollForwCommon.h"
#include "Forwarders.h"

namespace EvtCollForw
{
    class ICollForwPair : boost::noncopyable
    {
    public:
        virtual ~ICollForwPair() {}
        virtual bool CollectAndForward(boost::function0<bool> cancelRequested) = 0;
    };

    typedef boost::shared_ptr<ICollForwPair> PICollForwPair;
    typedef boost::shared_ptr<const ICollForwPair> PCICollForwPair;

    template <class TCollectedData, class TForwardedData, class TCommitData>
    class CollForwPair : public ICollForwPair
    {
    public:
        virtual ~CollForwPair() {}

        static PICollForwPair MakePair(
            boost::shared_ptr < CollectorBase <TCollectedData, TCommitData > > collector,
            boost::shared_ptr < ForwarderBase <TForwardedData, TCommitData > > forwarder)
        {
            // If there's no necessity for a converter, then the collected and forwarded data should be the same.
            // But note that, it doesn't mean that a converter is not needed, when both are same - ex: Xml string
            // is collected but say we need to still convert the format to JSON and so a converter would be necessary.
            BOOST_STATIC_ASSERT(boost::is_same<TCollectedData, TForwardedData>::value);

            return MakePair(collector, forwarder, boost::make_shared < NullConverter < TCollectedData > >());
        }

        static PICollForwPair MakePair(
            boost::shared_ptr < CollectorBase < TCollectedData, TCommitData > > collector,
            boost::shared_ptr < ForwarderBase < TForwardedData, TCommitData > > forwarder,
            boost::shared_ptr < IConverter < TCollectedData, TForwardedData > > converter)
        {
            boost::shared_ptr < CollForwPair < TCollectedData, TForwardedData, TCommitData > > toRet;

            try
            {
                toRet = boost::make_shared < CollForwPair < TCollectedData, TForwardedData, TCommitData > >();
            }
            catch (std::bad_alloc)
            {
                DebugPrintf(EvCF_LOG_ERROR, "Failed to allocate memory, while making collector-forwarder pair.\n");
                throw;
            }

            toRet->m_collector = collector;
            toRet->m_forwarder = forwarder;
            toRet->m_converter = converter;

            boost::function2<void, const TCommitData&, bool> commitCallback =
                boost::bind(&CollectorBase<TCollectedData, TCommitData>::Commit, collector, _1, _2);

            // TODO-SanKumar-NOW! This doesn't probably qualify for generic as well as doesn't seem
            // extensible. Rather give out a friend func : SetCallback().
            forwarder->Initialize(commitCallback);

            return toRet;
        }

        // TODO-SanKumar-1702: Log the cancellation and the position importantly.
#define CancelIfRequested() if (cancelRequested && cancelRequested()) { return false; }

        bool CollectAndForward(boost::function0<bool> cancelRequested)
        {

            TCollectedData collectedData;
            TForwardedData forwardedData;
            TCommitData newState;
            bool hasForwRequestedStop = false;

            CancelIfRequested();

            // TODO-SanKumar-1702: This would involve possibly I/O at collector. If so, should we pass
            // the cancelRequested to this method?
            if (m_collector->GetNext(collectedData, newState))
            {
                CancelIfRequested();

                // If converter is unable to work on the data/explicitly filtering out the data,
                // drop it and move forward.
                if (!m_converter->Convert(collectedData, forwardedData))
                    return true;

                CancelIfRequested();

                // TODO-SanKumar-1702: This would involve possibly I/O at forwarder and also commit at the
                // collector (in few cases). If so, should we pass the cancelRequested to this method?

                if (!m_forwarder->PutNext(forwardedData, newState))
                {
                    hasForwRequestedStop = true;
                    goto StopRequested;
                }

                CancelIfRequested();

                return true;
            }

        StopRequested:
            // TODO-SanKumar-1702: The Complete() routine might invoke a Commit on the collector. In
            // case of serialized runner, the ordering of this commit after all the other commits made
            // by the forwarder is guaranteed. But if there's any other type of runner defined (say
            // asyncForwardingRunner), then we should revisit this logic (in the  async example, we
            // might have to wait until all the data is processed by the forwarder).

            // Not performing a cancel requested at this point, since any completed progress at collector
            // and forwarder needn't go wasted.
            // CancelIfRequested();

            // TODO-SanKumar-NOW:
            // Explain, why shouldn't pass new state and why should pass new state....
            if (hasForwRequestedStop)
                m_forwarder->Complete();
            else
                m_forwarder->Complete(newState);

            return false;
        }

        // TODO-SanKumar-1702: Essentially, this should be made as private and must only be created
        // by the Runner factories using make_shared. But while introducing make_shared as friend of
        // this class works in VS 2013, it doesn't work in GCC.
        CollForwPair()
        {
        }

    private:
        boost::shared_ptr < CollectorBase < TCollectedData, TCommitData > > m_collector;
        boost::shared_ptr < ForwarderBase < TForwardedData, TCommitData > > m_forwarder;
        boost::shared_ptr < IConverter < TCollectedData, TForwardedData > > m_converter;
    };
}

#endif // !EVTCOLLFORW_PAIR_H
