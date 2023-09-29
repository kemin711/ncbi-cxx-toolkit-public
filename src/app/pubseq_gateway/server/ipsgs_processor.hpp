#ifndef IPSGS_PROCESSOR__HPP
#define IPSGS_PROCESSOR__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description: PSG processor interface
 *
 */

#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "psgs_uv_loop_binder.hpp"
#include "psgs_io_callbacks.hpp"
#include <objects/seqloc/Seq_id.hpp>

USING_NCBI_SCOPE;

/// Interface class (and self-factory) for request processor objects that can
/// retrieve data from a given data source.
/// The overal life cycle of the processors is as follows.
/// There is a one-time processors registration stage. On this stage a default
/// processor constructor will be used.
/// Then at the time when a request comes, all the registred processors will
/// receive the CreateProcessor(...) call. All not NULL processors will be
/// considered as those which are able to process the request.
/// Later the infrastructure will call the created processors Process() method
/// in parallel and periodically will call GetStatus() method. When all
/// processors finished it is considered as the end of the request processing.
///
/// There are a few agreements for the processors.
/// - The server replies use PSG protocol. When something is needed to be sent
///   to the client then m_Reply method should be used.
/// - When a processor is finished it should call the
///   SignalProcessorFinished() method.
/// - If a processor needs to do any logging then two thing need to be done:
///   - Set request context fot the current thread.
///   - Use one of the macro PSG_TRACE, PSG_INFO, PSG_WARNING, PSG_ERROR,
///     PSG_CRITICAL, PSG_MESSAGE (pubseq_gateway_logging.hpp)
///   - Reset request context
///   E.g.:
///   { CRequestContextResetter     context_resetter;
///     m_Request->SetRequestContext();
///     ...
///     PSG_WARNING("Something"); }
/// - The ProcessEvents() method can be called periodically (in addition to
///   some events like Cassandra data ready)
class IPSGS_Processor
{
public:
    /// The GetStatus() method returns a processor current status.
    /// The order is important: basing on it a worst (max) and best (min)
    /// status is calculated for a group of processors.
    enum EPSGS_Status {
        ePSGS_InProgress,   //< Processor is still working.
        ePSGS_Done,         //< Processor finished and found what needed.
        ePSGS_NotFound,     //< Processor finished and did not find anything.
        ePSGS_Canceled,     //< Processor finished because earlier it received
                            //< the Cancel() call.
        ePSGS_Timeout,      //< Processor finished because of a backend timeout.
        ePSGS_Error,        //< Processor finished and there was an error.
        ePSGS_Unauthorized  //< Processor finsihed and there was an authorization error.
    };

    /// Converts the processor status to a string for tracing and logging
    /// purposes.
    static string  StatusToString(EPSGS_Status  status);

    /// Converts the processor status to a string for protocol message.
    static string  StatusToProgressMessage(EPSGS_Status  status);

public:
    IPSGS_Processor() :
        m_FinishSignalled(false), m_UVThreadId(0),
        m_ProcessInvokeTimestampInitialized(false),
        m_SignalStartTimestampInitialized(false),
        m_SignalFinishTimestampInitialized(false)
    {}

    virtual ~IPSGS_Processor()
    {}

public:
    /// Tells if processor can process the given request
    /// @param request
    ///  PSG request to retrieve the data for. It is guaranteed to be not null.
    /// @param reply
    ///  The way to send reply chunks to the client. It is guaranteed to
    ///  be not null.
    /// @return
    ///  true if the processor can process the request
    virtual bool CanProcess(shared_ptr<CPSGS_Request> request,
                            shared_ptr<CPSGS_Reply> reply) const
    {
        return true;
    }

    /// Needs to be implemented only for the ID/get_na requests.
    /// It returns a list of named annotations which a processor recognizes as
    /// suitable for processing.
    /// @param request
    ///  PSG request to retrieve the data for. It is guaranteed to be not null.
    /// @param reply
    ///  The way to send reply chunks to the client. It is guaranteed to
    ///  be not null.
    /// @return
    ///  a list of annotations which can be processed
    virtual vector<string> WhatCanProcess(shared_ptr<CPSGS_Request> request,
                                          shared_ptr<CPSGS_Reply> reply) const
    {
        return vector<string>();
    }


    /// Create processor to fulfil PSG request using the data source
    /// @param request
    ///  PSG request to retrieve the data for. It is guaranteed to be not null.
    /// @param reply
    ///  The way to send reply chunks to the client. It is guaranteed to
    ///  be not null.
    /// @return
    ///  New processor object if this processor can theoretically fulfill
    ///  (all or a part of) the request; else NULL.
    virtual IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                             shared_ptr<CPSGS_Reply> reply,
                                             TProcessorPriority  priority) const = 0;

    /// Main processing function.
    /// It should avoid throwing exceptions. In case of errors it must make
    /// sure that:
    /// - the consequent GetStatus() calls return appropriate status
    /// - call SignalFinishProcessing() if there in no more processor activity
    /// If an exception is generated it is still a must for a processor to
    /// fulfill the obligations above. The dispatching code will log the
    /// message (and possibly trace) and continue in this case.
    virtual void Process(void) = 0;

    /// The infrastructure request to cancel processing
    virtual void Cancel(void) = 0;

    /// Tells the processor status (if it has finished or in progress)
    /// @return
    ///  the current processor status
    virtual EPSGS_Status GetStatus(void) = 0;

    /// Tells the processor name (used in logging and tracing)
    /// @return
    ///  the processor name
    virtual string GetName(void) const = 0;

    /// Tells the processor group name. For example, all the processors which
    /// retrieve data from Cassandra should return the same name in response to
    /// this call. This name is used to control the total number of
    /// simultaneously working processors retrieving from the same backend.
    /// @return
    ///  the processor group name
    virtual string GetGroupName(void) const = 0;

    /// Called when an event happened which may require to have
    /// some processing. By default nothing should be done.
    /// This method can be called as well on a timer event.
    virtual void ProcessEvent(void)
    {}

    /// Provides the user request
    /// @return
    ///  User request
    shared_ptr<CPSGS_Request> GetRequest(void) const
    {
        return m_Request;
    }

    /// Provides the reply wrapper
    /// @return
    ///  Reply wrapper which lets to send reply chunks to the user
    shared_ptr<CPSGS_Reply> GetReply(void) const
    {
        return m_Reply;
    }

    /// Provides the processor priority
    /// @return
    ///  The processor priority
    TProcessorPriority GetPriority(void) const
    {
        return m_Priority;
    }

    /// The provided callback will be called from the libuv loop assigned to
    /// the processor
    /// @param cb
    ///  The callback to be called from the libuv loop
    /// @param user_data
    ///  The data to be passed to the callback
    void PostponeInvoke(CPSGS_UvLoopBinder::TProcessorCB  cb,
                        void *  user_data);

    /// The provided callbacks will be called from the libuv loop assigned to
    /// the processor when the corresponding event appeared on the provided
    /// socket.
    /// @param fd
    ///  The socket to poll
    /// @param event
    ///  The event to wait for
    /// @param timeout_millisec
    ///  The timeout of waiting for the event
    /// @param user_data
    ///  The data to be passed to the callback
    /// @param event_cb
    ///  The event callback
    /// @param timeout_cb
    ///  The timeout callback
    /// @param error_cb
    ///  The error callback
    /// @note
    ///  The processor must make sure the socket is valid
    void SetSocketCallback(int  fd,
                           CPSGS_SocketIOCallback::EPSGS_Event  event,
                           uint64_t  timeout_millisec,
                           void *  user_data,
                           CPSGS_SocketIOCallback::TEventCB  event_cb,
                           CPSGS_SocketIOCallback::TTimeoutCB  timeout_cb,
                           CPSGS_SocketIOCallback::TErrorCB  error_cb);

    /// Saves the libuv worker thread id which runs the processor.
    /// To be used by the server framework only.
    /// @param uv_thread_id
    ///  The libuv worker thread id which runs the processor
    void SetUVThreadId(uv_thread_t  uv_thread_id)
    {
        m_UVThreadId = uv_thread_id;
    }

    /// Provides the libuv thread id which runs the processor.
    /// @return
    ///  The libuv worker thread id which runs the processor
    uv_thread_t GetUVThreadId(void) const
    {
        return m_UVThreadId;
    }

    /// Tells if a libuv thread id has been assigned to the processor.
    /// Basically the assignment of the libuv thread means that the processor
    /// has been started i.e. Process() was called before.
    /// @return
    ///  true if the libuv thread has been assigned
    bool IsUVThreadAssigned(void) const
    {
        return m_UVThreadId != 0;
    }

    /// Provides the timestamp of when the Process() method was called
    /// @return
    ///  Process() method invoke timestamp
    psg_time_point_t GetProcessInvokeTimestamp(bool &  is_valid) const
    {
        is_valid = m_ProcessInvokeTimestampInitialized;
        return m_ProcessInvokeTimestamp;
    }

    /// Provides the timestamp of when the processor called
    /// SignalStartProcessing() method
    /// @return
    ///  SignalStartProcessing() method invoke timestamp
    psg_time_point_t GetSignalStartTimestamp(bool &  is_valid) const
    {
        is_valid = m_SignalStartTimestampInitialized;
        return m_SignalStartTimestamp;
    }

    /// Provides the timestamp of when the processor called
    /// SignalFinishProcessing() method
    /// @return
    ///  SignalFinishProcessing() method invoke timestamp
    psg_time_point_t GetSignalFinishTimestamp(bool &  is_valid) const
    {
        is_valid = m_SignalFinishTimestampInitialized;
        return m_SignalFinishTimestamp;
    }

    /// Called just before the virtual Process() method is called
    void OnBeforeProcess(void);

public:
    /// Tells wether to continue or not after a processor called
    /// SignalStartProcessing() method.
    enum EPSGS_StartProcessing {
        ePSGS_Proceed,
        ePSGS_Cancel
    };

    /// A processor should call the method when it decides that it
    /// successfully started processing the request. The other processors
    /// which are handling this request in parallel will be cancelled.
    /// @return
    ///  The flag to continue or to stop further activity
    EPSGS_StartProcessing SignalStartProcessing(void);

    /// A processor should call this method when it decides that there is
    /// nothing else to be done.
    void SignalFinishProcessing(void);

public:
    /// Parse seq-id from a string and type representation.
    /// @param seq_id
    ///  Destination seq-id to place parsed value into.
    /// @param request_seq_id
    ///  Input string containing seq-id to parse.
    /// @param request_seq_id_type
    ///  Input seq-id type
    /// @param err_msg
    ///  Optional string to receive error message if any.
    /// @return
    ///  ePSGS_ParsedOK on success, ePSGS_ParseFailed otherwise.
    EPSGS_SeqIdParsingResult ParseInputSeqId(
        objects::CSeq_id& seq_id,
        const string& request_seq_id,
        int request_seq_id_type,
        string* err_msg = nullptr);

protected:
    bool GetEffectiveSeqIdType(
        const objects::CSeq_id& parsed_seq_id,
        int request_seq_id_type,
        int16_t& eff_seq_id_type,
        bool need_trace);

protected:
    shared_ptr<CPSGS_Request>  m_Request;
    shared_ptr<CPSGS_Reply>    m_Reply;
    TProcessorPriority         m_Priority;

protected:
    bool                        m_FinishSignalled;
    uv_thread_t                 m_UVThreadId;

private:
    bool                        m_ProcessInvokeTimestampInitialized;
    psg_time_point_t            m_ProcessInvokeTimestamp;

    bool                        m_SignalStartTimestampInitialized;
    psg_time_point_t            m_SignalStartTimestamp;

    bool                        m_SignalFinishTimestampInitialized;
    psg_time_point_t            m_SignalFinishTimestamp;
};


// Basically the logic is the same as in GetEffectiveSeqIdType() member
// This one does not send traces and does not provide the effective seq_id,
// just tells if there is not conflict between types
bool AreSeqIdTypesMatched(
    const objects::CSeq_id& parsed_seq_id,
    int request_seq_id_type);


#endif  // IPSGS_PROCESSOR__HPP

