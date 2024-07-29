/* $Id$
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
 * Author:  Repackaged by Chris Lanczycki from Charlie Liu's algTaxDataSource
 *
 * File Description:
 *
 *       Class to maintain lists of preferred and model tax nodes,
 *       using the Cdd-pref-nodes ASN specification.
 *
 * ===========================================================================
 */


#include <ncbi_pch.hpp>
#include <objects/cdd/Cdd_org_ref_set.hpp>
#include <algo/structure/cd_utils/cuPrefTaxNodes.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

const char* CPriorityTaxNodes::PREF_TAXNODE_FILE = "data/txnodes.asn";
   
CPriorityTaxNodes::CPriorityTaxNodes(TaxNodeInputType inputType) : m_inputType(inputType)
{
    string filename = PREF_TAXNODE_FILE;
    LoadFromFile(filename, false);
}

CPriorityTaxNodes::CPriorityTaxNodes(const string& prefTaxnodeFileName, TaxNodeInputType inputType) : m_inputType(inputType)
{
    LoadFromFile(prefTaxnodeFileName, false);
}

CPriorityTaxNodes::CPriorityTaxNodes(const CCdd_pref_nodes& prefNodes, TaxNodeInputType inputType) : m_inputType(inputType)
{
    BuildMap(prefNodes, false);
    m_loaded = true;
}


CPriorityTaxNodes::CPriorityTaxNodes(const vector< TTaxId >& taxids, TaxClient& taxClient, TaxNodeInputType inputType) : m_inputType(inputType) 
{
    CCdd_org_ref_set cddOrgRefSet;
    unsigned int nAdded = TaxIdsToCddOrgRefSet(taxids, cddOrgRefSet, taxClient);

    Reset();
    putIntoMap(cddOrgRefSet);
    m_loaded = (nAdded == taxids.size());
}

CPriorityTaxNodes::~CPriorityTaxNodes() 
{
}

void CPriorityTaxNodes::Reset(TaxNodeInputType* inputType, bool forceClearAncestorMap) {

	m_err = "";
    m_loaded = false;
    m_selectedTaxNodesMap.clear();

    if (forceClearAncestorMap || (inputType && !(m_inputType & *inputType))) {
        m_ancestralTaxNodeMap.clear();
    }

    if (inputType) {
        m_inputType = *inputType;
    }
}

    
bool CPriorityTaxNodes::LoadFromFile(const string& prefTaxnodeFileName, bool doReset)
{
    bool result = ReadPreferredTaxnodes(prefTaxnodeFileName, doReset);

    if (!result) 
		m_err = "Failed to read preferred Taxonomy nodes from file '" + prefTaxnodeFileName + "'.\n";

    m_loaded = result;
    return result;
}

unsigned int CPriorityTaxNodes::Load(const CCdd_pref_nodes& prefNodes, bool reset) 
{
    auto nInit = (reset) ? 0 : m_selectedTaxNodesMap.size();
    BuildMap(prefNodes, reset);
    return static_cast<unsigned int>(m_selectedTaxNodesMap.size() - nInit);
}
   
bool CPriorityTaxNodes::ReadPreferredTaxnodes(const string& filename, bool reset) 
{
    CCdd_pref_nodes prefNodes;
    if (!ReadASNFromFile(filename.c_str(), &prefNodes, false, &m_err))
	{
		return false;
	}

    BuildMap(prefNodes, reset);
    return true;
}

void CPriorityTaxNodes::BuildMap(const CCdd_pref_nodes& prefNodes, bool reset) {
    if (reset)
        Reset();

	//build a taxId/taxName map
	if ((m_inputType & eCddPrefNodes || m_inputType == eRawTaxIds) && prefNodes.CanGetPreferred_nodes())
		putIntoMap(prefNodes.GetPreferred_nodes());
	if ((m_inputType & eCddModelOrgs) && prefNodes.CanGetModel_organisms())
		putIntoMap(prefNodes.GetModel_organisms());
	if ((m_inputType & eCddOptional) && prefNodes.CanGetOptional_nodes())
		putIntoMap(prefNodes.GetOptional_nodes());
}

void CPriorityTaxNodes::putIntoMap(const CCdd_org_ref_set& orgRefs)
{
	const list< CRef< CCdd_org_ref > >& orgList = orgRefs.Get();
	list< CRef< CCdd_org_ref > >::const_iterator cit = orgList.begin();
	int i = static_cast<int>(m_selectedTaxNodesMap.size());
	for (; cit != orgList.end(); cit++)
	{
		m_selectedTaxNodesMap.insert(TaxidToOrgMap::value_type(getTaxId(*cit), OrgNode(i,*cit)));
		i++;
	}
}

string CPriorityTaxNodes::getTaxName(const CRef< CCdd_org_ref >& orgRef)
{
	if (orgRef->CanGetReference())
	{
		const COrg_ref& org = orgRef->GetReference();
        if (org.IsSetTaxname()) {
            return org.GetTaxname();
        }
	}

    return kEmptyStr;
}

TTaxId CPriorityTaxNodes::getTaxId(const CRef< CCdd_org_ref >& orgRef)
{
	if (orgRef->CanGetReference())
	{
		const COrg_ref& org = orgRef->GetReference();
		return org.GetTaxId();
	}
	else
		return ZERO_TAX_ID;
}

bool CPriorityTaxNodes::isActive(const CRef< CCdd_org_ref >& orgRef)
{
	return orgRef->GetActive();
}

unsigned int CPriorityTaxNodes::TaxIdsToCddOrgRefSet(const vector< TTaxId >& taxids, CCdd_org_ref_set& cddOrgRefSet, TaxClient& taxClient, vector<TTaxId>* notAddedTaxids) 
{

    unsigned int nAdded = 0, nTaxa = static_cast<unsigned int>(taxids.size());
    CCdd_org_ref_set::Tdata& cddOrgRefList = cddOrgRefSet.Set();

    if (notAddedTaxids) notAddedTaxids->clear();

    for (unsigned int i = 0; i < nTaxa; ++i) {
        CRef< CCdd_org_ref > cddOrgRef( new CCdd_org_ref );
        if (cddOrgRef.NotEmpty()) {
            COrg_ref& orgRef = cddOrgRef->SetReference();
            if (taxClient.GetOrgRef(taxids[i], orgRef)) {
//            CRef< CCdd_org_ref::TReference > orgRef( &cddOrgRef->SetReference());
//            if (taxClient.GetOrgRef(taxids[i], orgRef)) {
                cddOrgRef->SetActive(true);
                cddOrgRefList.push_back(cddOrgRef);
                ++nAdded;
            } else if (notAddedTaxids) {
                notAddedTaxids->push_back(taxids[i]);
            }
        } else if (notAddedTaxids) {
            notAddedTaxids->push_back(taxids[i]);
        }
    }
    return nAdded;
}

unsigned int CPriorityTaxNodes::CddOrgRefSetToTaxIds(const CCdd_org_ref_set& cddOrgRefSet, vector< TTaxId >& taxids, vector<int>* notAddedIndices) 
{
    TTaxId taxId;
    unsigned int taxaIndex = 0, nAdded = 0;
    const CCdd_org_ref_set::Tdata cddOrgRefList = cddOrgRefSet.Get();
    CCdd_org_ref_set::Tdata::const_iterator cddOrgRefListCit = cddOrgRefList.begin(), citEnd = cddOrgRefList.end();

    if (notAddedIndices) notAddedIndices->clear();

    for (; cddOrgRefListCit != citEnd; ++cddOrgRefListCit ) {
        taxId = getTaxId(*cddOrgRefListCit);
        if (taxId > ZERO_TAX_ID) {
            taxids.push_back(taxId);
            ++nAdded;
        } else if (notAddedIndices) {
            notAddedIndices->push_back(taxaIndex);
        }
        ++taxaIndex;
    }
    return nAdded;
}

TaxidToOrgMap::iterator CPriorityTaxNodes::findAncestor(TTaxId taxid, TaxClient* taxClient)
{
	TaxidToOrgMap::iterator titEnd = m_selectedTaxNodesMap.end(), tit = titEnd;
    TAncestorMap::iterator ancestorIt;

    if (taxid != ZERO_TAX_ID) {
        //  First see if this taxid has been seen before; if so, retrieve iterator from toMap...
        ancestorIt = m_ancestralTaxNodeMap.find(taxid);
        if (ancestorIt != m_ancestralTaxNodeMap.end() && ancestorIt->second >= ZERO_TAX_ID) {
            tit = m_selectedTaxNodesMap.find(ancestorIt->second);
        }

        //  If no ancestralMap, or ancestor not in ancestralMap, use taxClient if present.
        //  Add ancestral taxid to ancestralMap if found.
        if (taxClient && tit == titEnd) {
            for (tit = m_selectedTaxNodesMap.begin(); tit != titEnd; tit++)
            {
                if (taxClient->IsTaxDescendant(tit->first, taxid)) {
                    m_ancestralTaxNodeMap.insert(TAncestorMap::value_type(taxid, tit->first));
                    break;
                }
            }
        }
    }

	return tit;
}

bool CPriorityTaxNodes::IsPriorityTaxnode(TTaxId taxid) 
{
	TaxidToOrgMap::iterator it = m_selectedTaxNodesMap.find(taxid);	
    return it != m_selectedTaxNodesMap.end();
}

bool CPriorityTaxNodes::GetPriorityTaxid(TTaxId taxidIn, TTaxId& priorityTaxid, TaxClient& taxClient)
{
    string nodeName;
    return GetPriorityTaxidAndName(taxidIn, priorityTaxid, nodeName, taxClient);
}

bool CPriorityTaxNodes::GetPriorityTaxidAndName(TTaxId taxidIn, TTaxId& priorityTaxid, string& nodeName, TaxClient& taxClient)
{
    bool result = false;
	TaxidToOrgMap::iterator it = m_selectedTaxNodesMap.find(taxidIn), itEnd = m_selectedTaxNodesMap.end();

    priorityTaxid = ZERO_TAX_ID;
    nodeName = kEmptyStr;
    if (it != itEnd) {
        priorityTaxid = taxidIn;
        result = true;
    } else {  // fail to find exact match; try to find ancetral match
        it = findAncestor(taxidIn, &taxClient);
        if (it != itEnd)
        {
            priorityTaxid = it->first;
            result = true;
        }
    }

    if (it != itEnd) {  //  result = true
        nodeName = getTaxName(it->second.orgRef);
    }

	return result;
}

//  return -1 if fails or taxid = 0
int CPriorityTaxNodes::GetPriorityTaxnode(TTaxId taxid, const OrgNode*& orgNode, TaxClient* taxClient) 
{
	TaxidToOrgMap::iterator it = m_selectedTaxNodesMap.find(taxid), itEnd = m_selectedTaxNodesMap.end();	

    orgNode = NULL;
    if (taxid != ZERO_TAX_ID) {
        if (it != itEnd)
        {
            orgNode = &(it->second);
            return it->second.order;
        }
        else  // fail to find exact match; try to find ancetral match
        {
            it = findAncestor(taxid, taxClient);
            if (it != itEnd)
            {
                orgNode = &(it->second);
                return it->second.order;
            }
        }
    }
	return -1;
}

//  return index into list; -1 if fails
int CPriorityTaxNodes::GetPriorityTaxnode(TTaxId taxid, string& nodeName, TaxClient* taxClient) 
{
    const OrgNode* orgNode = NULL;

    nodeName = kEmptyStr;
    if (GetPriorityTaxnode(taxid, orgNode, taxClient) != -1 && orgNode) {
        nodeName.append(getTaxName(orgNode->orgRef));
        return orgNode->order;
    }
	return -1;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
