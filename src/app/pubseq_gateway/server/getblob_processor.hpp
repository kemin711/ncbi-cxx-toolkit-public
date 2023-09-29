#ifndef PSGS_GETBLOBPROCESSOR__HPP
#define PSGS_GETBLOBPROCESSOR__HPP

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
 * File Description: get blob processor
 *
 */

#include "cass_blob_base.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

// Forward declaration
class CCassFetch;


class CPSGS_GetBlobProcessor : public CPSGS_CassBlobBase
{
public:
    virtual bool CanProcess(shared_ptr<CPSGS_Request> request,
                            shared_ptr<CPSGS_Reply> reply) const;
    virtual IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                             shared_ptr<CPSGS_Reply> reply,
                                             TProcessorPriority  priority) const;
    virtual void Process(void);
    virtual EPSGS_Status GetStatus(void);
    virtual string GetName(void) const;
    virtual string GetGroupName(void) const;
    virtual void ProcessEvent(void);

public:
    CPSGS_GetBlobProcessor();
    CPSGS_GetBlobProcessor(shared_ptr<CPSGS_Request> request,
                           shared_ptr<CPSGS_Reply> reply,
                           TProcessorPriority  priority,
                           const SCass_BlobId &  blob_id);
    virtual ~CPSGS_GetBlobProcessor();

private:
    void OnGetBlobProp(CCassBlobFetch *  fetch_details,
                       CBlobRecord const &  blob, bool is_found);
    void OnGetBlobError(CCassBlobFetch *  fetch_details,
                        CRequestStatus::ECode  status, int  code,
                        EDiagSev  severity, const string &  message);
    void OnGetBlobChunk(CCassBlobFetch *  fetch_details,
                        CBlobRecord const &  blob,
                        const unsigned char *  chunk_data,
                        unsigned int  data_size, int  chunk_no);

private:
    void x_Process(void);
    void x_Peek(bool  need_wait);
    bool x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                bool  need_wait);

private:
    void x_OnMyNCBIError(const string &  cookie,
                         CRequestStatus::ECode  status,
                         int  code,
                         EDiagSev  severity,
                         const string &  message);
    void x_OnMyNCBIData(const string &  cookie,
                        CPSG_MyNCBIRequest_WhoAmI::SUserInfo info);

    SPSGS_BlobBySatSatKeyRequest *  m_BlobRequest;
};

#endif  // PSGS_GETBLOBPROCESSOR__HPP

