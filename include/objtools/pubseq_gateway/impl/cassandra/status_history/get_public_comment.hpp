#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__STATUS_HISTORY__GET_PUBLIC_COMMENT_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__STATUS_HISTORY__GET_PUBLIC_COMMENT_HPP

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors: Dmitrii Saprykin
 *
 * File Description:
 *
 * Task to resolve blob public comment from Cassandra
 *
 */

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include <memory>
#include <string>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/messages.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/status_history/record.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassStatusHistoryTaskGetPublicComment
    : public CCassBlobWaiter
{
    enum EBlobInserterState {
        eInit = 0,
        eStartReading,
        eReadingHistory,
        eReturnResult,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

 public:
    using TCommentCallback = function<void(string comment, bool isFound)>;

    CCassStatusHistoryTaskGetPublicComment(
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        CBlobRecord const &blob,
        TDataErrorCallback data_error_cb
    );

    void SetMessages(shared_ptr<CPSGMessages> messages);
    void SetCommentCallback(TCommentCallback callback);
    void SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver> callback);

 protected:
    void Wait1() override;

 private:
    void JumpToReplaced(CBlobRecord::TSatKey replaced);

    TCommentCallback m_CommentCallback{nullptr};
    shared_ptr<CPSGMessages> m_Messages{nullptr};
    TBlobFlagBase m_BlobFlags;
    TBlobStatusFlagsBase m_FirstHistoryFlags{-1};
    bool m_MatchingStatusRowFound{false};
    int64_t m_ReplacesRetries{-1};
    string m_PublicComment;
    int32_t m_CurrentKey{-1};
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__STATUS_HISTORY__GET_PUBLIC_COMMENT_HPP
