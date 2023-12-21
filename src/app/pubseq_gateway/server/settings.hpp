#ifndef SETTINGS__HPP
#define SETTINGS__HPP

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
 * File Description:
 *
 */

#include <corelib/ncbireg.hpp>

USING_NCBI_SCOPE;

class CPSGAlerts;


struct SPubseqGatewaySettings
{
    SPubseqGatewaySettings();
    ~SPubseqGatewaySettings();

    void Read(const CNcbiRegistry &   registry,
              CPSGAlerts &  alerts);
    void Validate(CPSGAlerts &  alerts);
    size_t GetProcessorMaxConcurrency(const CNcbiRegistry &   registry,
                                      const string &  processor_id);

    // [SERVER]
    unsigned short                      m_HttpPort;
    unsigned short                      m_HttpWorkers;
    unsigned int                        m_ListenerBacklog;
    unsigned short                      m_TcpMaxConn;
    unsigned int                        m_TimeoutMs;
    unsigned int                        m_MaxRetries;
    unsigned long                       m_SendBlobIfSmall;
    bool                                m_Log;
    int                                 m_MaxHops;
    double                              m_ResendTimeoutSec;
    double                              m_RequestTimeoutSec;
    size_t                              m_ProcessorMaxConcurrency;
    size_t                              m_SplitInfoBlobCacheSize;
    size_t                              m_ShutdownIfTooManyOpenFD;
    string                              m_RootKeyspace;
    string                              m_ConfigurationDomain;
    size_t                              m_HttpMaxBacklog;
    size_t                              m_HttpMaxRunning;
    size_t                              m_LogSamplingRatio;
    size_t                              m_LogTimingThreshold;

    // [STATISTICS]
    unsigned long                       m_SmallBlobSize;
    unsigned long                       m_MinStatValue;
    unsigned long                       m_MaxStatValue;
    unsigned long                       m_NStatBins;
    string                              m_StatScaleType;
    unsigned long                       m_TickSpan;
    string                              m_OnlyForProcessor;


    // [LMDB_CACHE]
    string                              m_Si2csiDbFile;
    string                              m_BioseqInfoDbFile;
    string                              m_BlobPropDbFile;

    // [AUTO_EXCLUDE]
    unsigned int                        m_ExcludeCacheMaxSize;
    unsigned int                        m_ExcludeCachePurgePercentage;
    unsigned int                        m_ExcludeCacheInactivityPurge;

    // [DEBUG]
    bool                                m_AllowIOTest;
    bool                                m_AllowProcessorTiming;

    // [IPG]
    int                                 m_IPGPageSize;
    bool                                m_EnableHugeIPG;

    // [ADMIN]
    string                              m_AuthToken;

    // [SSL]
    bool                                m_SSLEnable;
    string                              m_SSLCertFile;
    string                              m_SSLKeyFile;
    string                              m_SSLCiphers;

    // [HEALTH]
    string                              m_TestSeqId;
    bool                                m_TestSeqIdIgnoreError;

    // [CASSANDRA_PROCESSOR]
    bool                                m_CassandraProcessorsEnabled;

    // [OSG_PROCESSOR]
    bool                                m_OSGProcessorsEnabled;

    // [CDD_PROCESSOR]
    bool                                m_CDDProcessorsEnabled;

    // [WGS_PROCESSOR]
    bool                                m_WGSProcessorsEnabled;

    // [SNP_PROCESSOR]
    bool                                m_SNPProcessorsEnabled;

    // [COUNTERS]
    // Configured counter/statistics ID to name/description
    map<string, tuple<string, string>>  m_IdToNameAndDescription;

    // [MY_NCBI]
    size_t                              m_MyNCBIOKCacheSize;
    size_t                              m_MyNCBINotFoundCacheSize;
    size_t                              m_MyNCBINotFoundCacheExpirationSec;
    size_t                              m_MyNCBIErrorCacheSize;
    size_t                              m_MyNCBIErrorCacheBackOffMs;
    string                              m_MyNCBIURL;
    string                              m_MyNCBIHttpProxy;
    size_t                              m_MyNCBITimeoutMs;
    size_t                              m_MyNCBIResolveTimeoutMs;

private:
    void x_ReadServerSection(const CNcbiRegistry &   registry);
    void x_ReadStatisticsSection(const CNcbiRegistry &   registry);
    void x_ReadLmdbCacheSection(const CNcbiRegistry &   registry);
    void x_ReadAutoExcludeSection(const CNcbiRegistry &   registry);
    void x_ReadDebugSection(const CNcbiRegistry &   registry);
    void x_ReadIPGSection(const CNcbiRegistry &   registry);
    void x_ReadAdminSection(const CNcbiRegistry &   registry,
                            CPSGAlerts &  alerts);
    void x_ReadSSLSection(const CNcbiRegistry &   registry);
    void x_ReadHealthSection(const CNcbiRegistry &   registry);
    void x_ReadOSGProcessorSection(const CNcbiRegistry &   registry);
    void x_ReadCassandraProcessorSection(const CNcbiRegistry &   registry);
    void x_ReadCDDProcessorSection(const CNcbiRegistry &   registry);
    void x_ReadWGSProcessorSection(const CNcbiRegistry &   registry);
    void x_ReadSNPProcessorSection(const CNcbiRegistry &   registry);
    void x_ReadMyNCBISection(const CNcbiRegistry &   registry);
    void x_ReadCountersSection(const CNcbiRegistry &   registry);

    unsigned long x_GetDataSize(const CNcbiRegistry &  registry,
                                const string &  section,
                                const string &  entry,
                                unsigned long  default_val);
};


#endif

