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

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/status_history/get_public_comment.hpp>

#include <memory>
#include <string>
#include <utility>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

BEGIN_SCOPE()

constexpr int64_t kMaxReplacesRetries = 5;
constexpr TBlobStatusFlagsBase kWithdrawnMask =
    static_cast<TBlobStatusFlagsBase>(EBlobStatusFlags::eWithdrawn) +
    static_cast<TBlobStatusFlagsBase>(EBlobStatusFlags::eWithdrawnPermanently);
const char * kDefaultSuppressedMessage = "BLOB_STATUS_SUPPRESSED";
const char * kDefaultWithdrawnMessage= "BLOB_STATUS_WITHDRAWN";

bool IsBlobWithdrawn(TBlobFlagBase flags) {
    return (flags & static_cast<TBlobFlagBase>(EBlobFlags::eWithdrawn)) != 0;
}

bool IsBlobSuppressed(TBlobFlagBase flags) {
    return (flags & static_cast<TBlobFlagBase>(EBlobFlags::eSuppress)) != 0;
}

bool SameWithdrawn(TBlobStatusFlagsBase a, TBlobStatusFlagsBase b) {
    return (a & kWithdrawnMask) == (b & kWithdrawnMask);
}

bool IsHistorySuppressed(TBlobStatusFlagsBase flags) {
    return (flags & static_cast<TBlobStatusFlagsBase>(EBlobStatusFlags::eSuppressPermanently)) != 0;
}

END_SCOPE()

CCassStatusHistoryTaskGetPublicComment::CCassStatusHistoryTaskGetPublicComment(
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    CBlobRecord const &blob,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(move(conn), keyspace, blob.GetKey(), true, move(data_error_cb))
    , m_BlobFlags(blob.GetFlags())
    , m_ReplacesRetries(kMaxReplacesRetries)
    , m_CurrentKey(blob.GetKey())
{}

void CCassStatusHistoryTaskGetPublicComment::SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver>  callback)
{
    if (callback && m_State != eInit) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CCassStatusHistoryTaskGetPublicComment: DataReadyCB can't be assigned "
           "after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB3(callback);
}

void CCassStatusHistoryTaskGetPublicComment::JumpToReplaced(CBlobRecord::TSatKey replaced)
{
    --m_ReplacesRetries;
    m_CurrentKey = replaced;
    m_MatchingStatusRowFound = false;
    m_State = eStartReading;
    m_PublicComment.clear();
}

void CCassStatusHistoryTaskGetPublicComment::SetMessages(shared_ptr<CPSGMessages> messages)
{
    m_Messages = move(messages);
}

void CCassStatusHistoryTaskGetPublicComment::SetCommentCallback(TCommentCallback callback)
{
    m_CommentCallback = move(callback);
}

void CCassStatusHistoryTaskGetPublicComment::Wait1()
{
    bool b_need_repeat;
    do {
        b_need_repeat = false;
        switch (m_State) {
            case eError:
            case eDone:
                return;

            case eInit: {
                if (!IsBlobSuppressed(m_BlobFlags) && !IsBlobWithdrawn(m_BlobFlags)) {
                    if (m_CommentCallback) {
                        m_CommentCallback("", false);
                    }
                    m_State = eDone;
                } else {
                    m_State = eStartReading;
                    b_need_repeat = true;
                }
                break;
            }

            case eStartReading: {
                CloseAll();
                m_QueryArr.clear();
                m_QueryArr.push_back({m_Conn->NewQuery(), 0});
                auto query = m_QueryArr[0].query;
                string sql =
                    "SELECT flags, public_comment, replaces "
                    "FROM " + GetKeySpace() + ".blob_status_history WHERE sat_key = ?";
                query->SetSQL(sql, 1);
                query->BindInt32(0, m_CurrentKey);
                SetupQueryCB3(query);
                query->Query(GetQueryConsistency(), m_Async, true);
                m_State = eReadingHistory;
                break;
            }

            case eReadingHistory: {
                auto query = m_QueryArr[0].query;
                if (CheckReady(m_QueryArr[0])) {
                    while (m_State == eReadingHistory && query->NextRow() == ar_dataready) {
                        int64_t flags = query->FieldGetInt64Value(0, 0);
                        string comment = query->FieldGetStrValueDef(1, "");
                        CBlobRecord::TSatKey replaces = query->FieldGetInt32Value(2, 0);

                        // blob_prop does not have full withdrawn representation so
                        // as a workaround we use first history record flags
                        if (m_FirstHistoryFlags == -1) {
                            m_FirstHistoryFlags = flags;
                        }
                        // blob is withdrawn
                        if (IsBlobWithdrawn(m_BlobFlags)) {
                            if (!SameWithdrawn(flags, m_FirstHistoryFlags)) {
                                if (m_MatchingStatusRowFound) {
                                    m_State = eReturnResult;
                                } else if (replaces > 0 && m_ReplacesRetries > 0) {
                                    JumpToReplaced(replaces);
                                } else {
                                    m_State = eReturnResult;
                                }
                                b_need_repeat = true;
                            } else {
                                m_MatchingStatusRowFound = true;
                                m_PublicComment = comment;
                            }
                        }
                        // blob is suppressed
                        else {
                            if (!IsHistorySuppressed(flags)) {
                                if (m_MatchingStatusRowFound) {
                                    m_State = eReturnResult;
                                } else if (replaces > 0 && m_ReplacesRetries > 0) {
                                    JumpToReplaced(replaces);
                                } else {
                                    m_State = eReturnResult;
                                }
                                b_need_repeat = true;
                            } else {
                                m_MatchingStatusRowFound = true;
                                m_PublicComment = comment;
                            }
                        }
                    }
                    if (query->IsEOF()) {
                        m_State = eReturnResult;
                        b_need_repeat = true;
                    }
                }
                break;
            }

            case eReturnResult: {
                CloseAll();
                if (m_CommentCallback) {
                    if (m_PublicComment.empty()) {
                        if (m_Messages != nullptr) {
                            string comment;
                            const char * message_type = nullptr;
                            if (IsBlobSuppressed(m_BlobFlags)) {
                                comment = m_Messages->Get(kDefaultSuppressedMessage);
                                message_type = kDefaultSuppressedMessage;
                            } else if (IsBlobWithdrawn(m_BlobFlags)) {
                                comment = m_Messages->Get(kDefaultWithdrawnMessage);
                                message_type = kDefaultWithdrawnMessage;
                            }
                            if (comment.empty() && message_type != nullptr) {
                                char msg[1024];
                                snprintf(msg, sizeof(msg), "Message is empty for (%s)", message_type);
                                Error(CRequestStatus::e502_BadGateway, CCassandraException::eMissData, eDiag_Error, msg);
                            } else {
                                m_CommentCallback(move(comment), true);
                            }
                        } else {
                            Error(CRequestStatus::e502_BadGateway, CCassandraException::eMissData,
                                eDiag_Error, "Messages provider not configured for Public Comment retrieval");
                        }
                    } else {
                        m_CommentCallback(move(m_PublicComment), true);
                    }
                }
                m_State = eDone;
                break;
            }

            default: {
                char msg[1024];
                string keyspace = GetKeySpace();
                snprintf(msg, sizeof(msg), "Failed to get public comment for record (key=%s.%d) unexpected state (%d)",
                    keyspace.c_str(), GetKey(), static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}

END_IDBLOB_SCOPE
