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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   PSG server request time series statistics
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "time_series_stat.hpp"


CMomentousCounterSeries::CMomentousCounterSeries() :
    m_Accumulated(0), m_AccumulatedCount(0),
    m_TotalValues(0.0),
    m_MaxValue(0.0),
    m_Loop(false),
    m_TotalMinutesCollected(1),
    m_CurrentIndex(0)
{
    Reset();
}


void CMomentousCounterSeries::Add(uint64_t   value)
{
    // Adding happens every 5 seconds and goes to the accumulated values
    m_Accumulated += value;
    ++m_AccumulatedCount;
}


void CMomentousCounterSeries::Rotate(void)
{
    // Rotate should:
    // - calculate avarage
    // - store it in the current index cell
    // - rotate the current index

    size_t      current_index = m_CurrentIndex.load();
    m_Values[current_index] = double(m_Accumulated) / double(m_AccumulatedCount);
    m_TotalValues += m_Values[current_index];
    if (m_Values[current_index] > m_MaxValue) {
        m_MaxValue = m_Values[current_index];
    }

    m_Accumulated = 0;
    m_AccumulatedCount = 0;

    size_t      new_current_index = m_CurrentIndex.load();
    if (new_current_index == kSeriesIntervals - 1) {
        new_current_index = 0;
    } else {
        ++new_current_index;
    }

    m_Values[new_current_index] = 0;

    m_CurrentIndex.store(new_current_index);
    ++m_TotalMinutesCollected;
    if (new_current_index == 0) {
        m_Loop = true;
    }
}


void CMomentousCounterSeries::Reset(void)
{
    for (size_t  k = 0; k < kSeriesIntervals; ++k)
        m_Values[k] = 0.0;
    m_TotalValues = 0.0;
    m_MaxValue = 0.0;

    m_Accumulated = 0;
    m_AccumulatedCount = 0;

    m_CurrentIndex.store(0);
    m_TotalMinutesCollected.store(1);
    m_Loop = false;
}


CJsonNode
CMomentousCounterSeries::Serialize(const vector<pair<int, int>> &  time_series,
                                   bool  loop, size_t  current_index) const
{
    CJsonNode   ret(CJsonNode::NewObjectNode());

    ret.SetByKey("AverageValues",
                 x_SerializeOneSeries(time_series, loop, current_index));
    return ret;
}


CJsonNode  CMomentousCounterSeries::x_SerializeOneSeries(const vector<pair<int, int>> &  time_series,
                                                         bool  loop,
                                                         size_t  current_index) const
{
    CJsonNode   ret(CJsonNode::NewObjectNode());

    if (current_index == 0 && loop == false) {
        // There is no data collected yet
        return ret;
    }

    CJsonNode   output_series(CJsonNode::NewArrayNode());

    // Index in the array where the data are collected
    size_t      raw_index;
    if (current_index == 0) {
        raw_index = kSeriesIntervals - 1;
        loop = false;   // to avoid going the second time over
    } else {
        raw_index = current_index - 1;
    }

    size_t      current_accumulated_mins = 0;
    double      current_accumulated_vals = 0;
    size_t      output_data_index = 0;
    double      total_processed_vals = 0.0;

    // The current index in the 'time_series', i.e. a pair of
    // <mins to accumulate>:<last sequential data index>
    // It is guaranteed they are both > 0.
    size_t      range_index = 0;
    size_t      current_mins_to_accumulate = time_series[range_index].first;
    size_t      current_last_seq_index = time_series[range_index].second;

    for ( ;; ) {
        double  val = m_Values[raw_index];

        ++current_accumulated_mins;
        current_accumulated_vals += val;
        total_processed_vals += val;

        if (current_accumulated_mins >= current_mins_to_accumulate) {
            output_series.AppendDouble(double(current_accumulated_vals) /
                                       double(current_accumulated_mins));
            current_accumulated_mins = 0;
            current_accumulated_vals = 0.0;
        }

        ++output_data_index;
        if (output_data_index > current_last_seq_index) {
            ++range_index;
            current_mins_to_accumulate = time_series[range_index].first;
            current_last_seq_index = time_series[range_index].second;
        }

        if (raw_index == 0)
            break;
        --raw_index;
    }

    if (loop) {
        raw_index = kSeriesIntervals - 1;
        while (raw_index > current_index + 1) {
            double    val = m_Values[raw_index];
            --raw_index;

            ++current_accumulated_mins;
            current_accumulated_vals += val;
            total_processed_vals += val;

            if (current_accumulated_mins >= current_mins_to_accumulate) {
                output_series.AppendDouble(double(current_accumulated_vals) /
                                           double(current_accumulated_mins));
                current_accumulated_mins = 0;
                current_accumulated_vals = 0;
            }

            ++output_data_index;
            if (output_data_index > current_last_seq_index) {
                ++range_index;
                current_mins_to_accumulate = time_series[range_index].first;
                current_last_seq_index = time_series[range_index].second;
            }
        }
    }

    if (current_accumulated_mins > 0) {
        output_series.AppendDouble(double(current_accumulated_vals) /
                                   double(current_accumulated_mins));
    }

    if (loop) {
        // The current minute and the last minute in case of a loop are not
        // sent to avoid unreliable data
        uint64_t    rest_mins = m_TotalMinutesCollected.load() - kSeriesIntervals - 2;
        double      rest_vals = m_TotalValues - total_processed_vals - m_Values[current_index];

        if (rest_mins > 0) {
            ret.SetDouble("RestAverageValue", rest_vals / double(rest_mins));
        } else {
            ret.SetDouble("RestAverageValue", 0.0);
        }
    } else {
        ret.SetDouble("RestAverageValue", 0.0);
    }

    ret.SetDouble("Max", m_MaxValue);
    if (m_TotalMinutesCollected == 1) {
        // That's the very beginning; the first minute is still accumulating
        ret.SetDouble("Avg", 0.0);
    } else {
        ret.SetDouble("Avg", m_TotalValues / (m_TotalMinutesCollected.load() - 1));
    }
    ret.SetByKey("time_series", output_series);
    return ret;
}


CProcessorRequestTimeSeries::CProcessorRequestTimeSeries() :
    m_Loop(false),
    m_TotalMinutesCollected(1),
    m_CurrentIndex(0)
{
    Reset();
}


void CProcessorRequestTimeSeries::Add(void)
{
    size_t      current_index = m_CurrentIndex.load();
    ++m_Requests[current_index];
    ++m_TotalRequests;
}


void CProcessorRequestTimeSeries::Rotate(void)
{
    size_t      new_current_index = m_CurrentIndex.load();
    if (new_current_index == kSeriesIntervals - 1) {
        new_current_index = 0;
    } else {
        ++new_current_index;
    }

    m_Requests[new_current_index] = 0;

    m_CurrentIndex.store(new_current_index);
    ++m_TotalMinutesCollected;
    if (new_current_index == 0) {
        m_Loop = true;
    }
}


void CProcessorRequestTimeSeries::Reset(void)
{
    memset(m_Requests, 0, sizeof(m_Requests));
    m_TotalRequests = 0;

    m_CurrentIndex.store(0);
    m_TotalMinutesCollected.store(1);
    m_Loop = false;
}


CJsonNode  CProcessorRequestTimeSeries::Serialize(const vector<pair<int, int>> &  time_series,
                                                  bool  loop, size_t  current_index) const
{
    CJsonNode   ret(CJsonNode::NewObjectNode());

    ret.SetByKey("Requests",
                 x_SerializeOneSeries(m_Requests, m_TotalRequests,
                                      time_series, loop, current_index));
    return ret;
}


CJsonNode  CProcessorRequestTimeSeries::x_SerializeOneSeries(const uint64_t *  values,
                                                             uint64_t  grand_total,
                                                             const vector<pair<int, int>> &  time_series,
                                                             bool  loop,
                                                             size_t  current_index) const
{
    CJsonNode   ret(CJsonNode::NewObjectNode());

    if (current_index == 0 && loop == false) {
        // There is no data collected yet
        return ret;
    }

    CJsonNode   output_series(CJsonNode::NewArrayNode());

    // Needed to calculate max and average reqs/sec
    uint64_t    max_n_req_per_min = 0;
    uint64_t    total_reqs = 0;
    uint64_t    total_mins = 0;

    // Index in the array where the data are collected
    size_t      raw_index;
    if (current_index == 0) {
        raw_index = kSeriesIntervals - 1;
        loop = false;   // to avoid going the second time over
    } else {
        raw_index = current_index - 1;
    }

    size_t      current_accumulated_mins = 0;
    uint64_t    current_accumulated_reqs = 0;
    size_t      output_data_index = 0;

    // The current index in the 'time_series', i.e. a pair of
    // <mins to accumulate>:<last sequential data index>
    // It is guaranteed they are both > 0.
    size_t      range_index = 0;
    size_t      current_mins_to_accumulate = time_series[range_index].first;
    size_t      current_last_seq_index = time_series[range_index].second;

    for ( ;; ) {
        uint64_t    reqs = values[raw_index];

        ++total_mins;
        max_n_req_per_min = max(max_n_req_per_min, reqs);
        total_reqs += reqs;

        ++current_accumulated_mins;
        current_accumulated_reqs += reqs;

        if (current_accumulated_mins >= current_mins_to_accumulate) {
            output_series.AppendDouble(double(current_accumulated_reqs) /
                                       (double(current_accumulated_mins) * 60.0));
            current_accumulated_mins = 0;
            current_accumulated_reqs = 0;
        }

        ++output_data_index;
        if (output_data_index > current_last_seq_index) {
            ++range_index;
            current_mins_to_accumulate = time_series[range_index].first;
            current_last_seq_index = time_series[range_index].second;
        }

        if (raw_index == 0)
            break;
        --raw_index;
    }

    if (loop) {
        raw_index = kSeriesIntervals - 1;
        while (raw_index > current_index + 1) {
            uint64_t    reqs = values[raw_index];
            --raw_index;

            ++total_mins;
            max_n_req_per_min = max(max_n_req_per_min, reqs);
            total_reqs += reqs;

            ++current_accumulated_mins;
            current_accumulated_reqs += reqs;

            if (current_accumulated_mins >= current_mins_to_accumulate) {
                output_series.AppendDouble(double(current_accumulated_reqs) /
                                           (double(current_accumulated_mins) * 60.0));
                current_accumulated_mins = 0;
                current_accumulated_reqs = 0;
            }

            ++output_data_index;
            if (output_data_index > current_last_seq_index) {
                ++range_index;
                current_mins_to_accumulate = time_series[range_index].first;
                current_last_seq_index = time_series[range_index].second;
            }
        }
    }

    if (current_accumulated_mins > 0) {
        output_series.AppendDouble(double(current_accumulated_reqs) /
                                   (double(current_accumulated_mins) * 60.0));
    }

    if (loop) {
        size_t      last_minute_index = current_index + 1;
        if (last_minute_index >= kSeriesIntervals)
            last_minute_index = 0;

        // The current minute and the last minute in case of a loop are not
        // sent to avoid unreliable data
        uint64_t    rest_reqs = grand_total - values[last_minute_index] - values[current_index];
        uint64_t    rest_mins = m_TotalMinutesCollected.load() - kSeriesIntervals - 2;

        if (rest_mins > 0) {
            ret.SetDouble("RestAvgReqPerSec", rest_reqs / (rest_mins * 60.0));
        } else {
            ret.SetDouble("RestAvgReqPerSec", 0.0);
        }
    } else {
        ret.SetDouble("RestAvgReqPerSec", 0.0);
    }

    ret.SetInteger("TotalRequests", total_reqs);
    ret.SetDouble("MaxReqPerSec", max_n_req_per_min / 60.0);
    ret.SetDouble("AvgReqPerSec", total_reqs / (total_mins * 60.0));
    ret.SetByKey("time_series", output_series);

    // Just in case: grand total includes everything - sent minutes, not sent
    // minutes in case of the loops and the rest
    ret.SetInteger("GrandTotalRequests", grand_total);
    return ret;
}




// Converts a request status to the counter in the time series
// The logic matches the logic in GRID dashboard
CRequestTimeSeries::EPSGSCounter
CRequestTimeSeries::RequestStatusToCounter(CRequestStatus::ECode  status)
{
    if (status == CRequestStatus::e404_NotFound)
        return eNotFound;

    if (status >= CRequestStatus::e500_InternalServerError)
        return eError;

    if (status >= CRequestStatus::e400_BadRequest)
        return eWarning;

    return eRequest;
}


CRequestTimeSeries::CRequestTimeSeries()
{
    Reset();
}


void CRequestTimeSeries::Add(EPSGSCounter  counter)
{
    size_t      current_index = m_CurrentIndex.load();
    switch (counter) {
        case eRequest:
            ++m_Requests[current_index];
            ++m_TotalRequests;
            break;
        case eError:
            ++m_Errors[current_index];
            ++m_TotalErrors;
            break;
        case eWarning:
            ++m_Warnings[current_index];
            ++m_TotalWarnings;
            break;
        case eNotFound:
            ++m_NotFound[current_index];
            ++m_TotalNotFound;
            break;
        default:
            break;
    }
}


void CRequestTimeSeries::Rotate(void)
{
    size_t      new_current_index = m_CurrentIndex.load();
    if (new_current_index == kSeriesIntervals - 1) {
        new_current_index = 0;
    } else {
        ++new_current_index;
    }

    m_Requests[new_current_index] = 0;
    m_Errors[new_current_index] = 0;
    m_Warnings[new_current_index] = 0;
    m_NotFound[new_current_index] = 0;

    m_CurrentIndex.store(new_current_index);
    ++m_TotalMinutesCollected;
    if (new_current_index == 0) {
        m_Loop = true;
    }
}


void CRequestTimeSeries::Reset(void)
{
    memset(m_Requests, 0, sizeof(m_Requests));
    m_TotalRequests = 0;
    memset(m_Errors, 0, sizeof(m_Errors));
    m_TotalErrors = 0;
    memset(m_Warnings, 0, sizeof(m_Warnings));
    m_TotalWarnings = 0;
    memset(m_NotFound, 0, sizeof(m_NotFound));
    m_TotalNotFound = 0;

    m_CurrentIndex.store(0);
    m_TotalMinutesCollected.store(1);
    m_Loop = false;
}


CJsonNode  CRequestTimeSeries::Serialize(const vector<pair<int, int>> &  time_series,
                                         bool  loop, size_t  current_index) const
{
    CJsonNode   ret(CJsonNode::NewObjectNode());

    ret.SetByKey("Requests",
                 x_SerializeOneSeries(m_Requests, m_TotalRequests,
                                      time_series, loop, current_index));
    ret.SetByKey("Errors",
                 x_SerializeOneSeries(m_Errors, m_TotalErrors,
                                      time_series, loop, current_index));
    ret.SetByKey("Warnings",
                 x_SerializeOneSeries(m_Warnings, m_TotalWarnings,
                                      time_series, loop, current_index));
    ret.SetByKey("NotFound",
                 x_SerializeOneSeries(m_NotFound, m_TotalNotFound,
                                      time_series, loop, current_index));
    return ret;
}

