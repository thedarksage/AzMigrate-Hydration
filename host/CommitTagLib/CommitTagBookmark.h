#ifndef COMMIT_TAG_BOOKMARK_H
#define COMMIT_TAG_BOOKMARK_H

#include "json_writer.h"
#include "json_reader.h"
#include <string>
#include <map>

namespace RcmClientLib {
    class ReplicationBookmarkInput {
    public:
        std::string BookmarkType;

        std::string BookmarkId;

    };

    class CommitTagBookmarkInput {
    public:

        ReplicationBookmarkInput BookmarkInput;

        CommitTagBookmarkInput() {}

        CommitTagBookmarkInput(const std::string &BookmarkType,
            const std::string &BookmarkId)
        {
            BookmarkInput.BookmarkType = BookmarkType;
            BookmarkInput.BookmarkId = BookmarkId;
        }

    };

    class FinalReplicationBookmarkDetails {
    public:

        /// \brief tag time in 100 nano secs since 1601 (.Net epoch)
        uint64_t BookmarkTime;

        uint64_t BookmarkSequenceId;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "FinalReplicationBookmarkDetails", false);

            JSON_E(adapter, BookmarkTime);
            JSON_T(adapter, BookmarkSequenceId);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, BookmarkTime);
            JSON_P(node, BookmarkSequenceId);
        }
    };

    class ReplicationBookmarkOutput : public ReplicationBookmarkInput {
    public:

       std::map<std::string, FinalReplicationBookmarkDetails> SourceDiskIdToBookmarkDetailsMap;

       void serialize(JSON::Adapter& adapter)
       {
            JSON::Class root(adapter, "ReplicationBookmarkOutput", false);

            JSON_E(adapter, BookmarkType);
            JSON_E(adapter, BookmarkId);
            JSON_KV_T(adapter, "SourceDiskIdToBookmarkDetailsMap", SourceDiskIdToBookmarkDetailsMap);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, BookmarkType);
            JSON_P(node, BookmarkId);
            JSON_KV_CL(node, SourceDiskIdToBookmarkDetailsMap);
        }

    };

    class CommitTagBookmarkOutput {
    public:

        ReplicationBookmarkOutput BookmarkOutput;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "CommitTagBookmarkOutput", false);

            JSON_T(adapter, BookmarkOutput);
        }

        void serialize(ptree& node)
        {
            JSON_CL(node, BookmarkOutput);
        }
    };
}

#endif
