#ifndef PSGS_ANNOTPROCESSOR__HPP
#define PSGS_ANNOTPROCESSOR__HPP

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
 * File Description: named annotation processor
 *
 */

#include "cass_blob_base.hpp"
#include "resolve_base.hpp"

#include <objtools/pubseq_gateway/impl/cassandra/nannot/filter.hpp>

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

// Forward declaration
class CCassFetch;

class CPSGS_AnnotProcessor : public CPSGS_ResolveBase,
                             public CPSGS_CassBlobBase
{
public:
    virtual bool CanProcess(shared_ptr<CPSGS_Request> request,
                            shared_ptr<CPSGS_Reply> reply) const;
    virtual vector<string> WhatCanProcess(shared_ptr<CPSGS_Request> request,
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
    CPSGS_AnnotProcessor();
    CPSGS_AnnotProcessor(shared_ptr<CPSGS_Request> request,
                         shared_ptr<CPSGS_Reply> reply,
                         TProcessorPriority  priority);
    virtual ~CPSGS_AnnotProcessor();

private:
    static vector<string> x_FilterNames(const vector<string> &  names);
    static bool x_IsNameValid(const string &  name);

    void x_OnResolutionGoodData(void);
    void x_OnSeqIdResolveError(
                        CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message);
    void x_OnSeqIdResolveFinished(
                        SBioseqResolution &&  bioseq_resolution);
    void x_SendBioseqInfo(SBioseqResolution &  bioseq_resolution);

private:
    bool x_OnNamedAnnotData(CNAnnotRecord &&  annot_record,
                            bool  last,
                            CCassNamedAnnotFetch *  fetch_details,
                            int32_t  sat);
    void x_OnNamedAnnotError(CCassNamedAnnotFetch *  fetch_details,
                             CRequestStatus::ECode  status,
                             int  code,
                             EDiagSev  severity,
                             const string &  message);
    void x_SendAnnotDataToClient(CNAnnotRecord &&  annot_record, int32_t  sat);

private:
    bool x_NeedToRequestBlobProp(void);
    void x_RequestBlobProp(int32_t  sat, int32_t  sat_key, int64_t  last_modified);

    void OnAnnotBlobProp(CCassBlobFetch *  fetch_details,
                         CBlobRecord const &  blob, bool is_found);
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
    void x_Peek(bool  need_wait);
    bool x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                bool  need_wait);

private:
    // The processor filters out some of the requested named annotations
    // This vector holds those which can be processed
    vector<string>              m_ValidNames;

    SPSGS_AnnotRequest *        m_AnnotRequest;

    bool                        m_BlobStage;

    // @temp ID-7327 NAnnot storage schema is being changed. Data is in migration progress. During that time
    // there expected to exist duplicates between NAnnot keyspaces. Keyspace with larger sat_id has priority in that case
    // First migration NAnnotG2 => NAnnotG3 will cause duplicates between NAnnotG3 and NAnnotG2
    // Later migration NANnotG => NAnnotG2 will cause duplicates between NAnnotG2 and NAnnotG
    // This component should be removed when migration is finished
    unique_ptr<CNAnnotFilter>   m_AnnotFilter;

    // The list of names which have been received and (possibly) sent.
    // It is used to calculate not found and error/timeout annotations
    set<string>                 m_Success;
};

#endif  // PSGS_RESOLVEPROCESSOR__HPP

