/**
 *  @file   PandoraSDK/src/Managers/Metadata.cc
 * 
 *  @brief  Implementation of metadata classes.
 * 
 *  $Log: $
 */

#include "Managers/Metadata.h"

namespace pandora
{

CaloHitMetadata::CaloHitMetadata(CaloHitList *const pCaloHitList, const std::string &caloHitListName, const bool initialHitAvailability) :
    m_pCaloHitList(pCaloHitList),
    m_caloHitListName(caloHitListName)
{
    for (CaloHitList::const_iterator hitIter = pCaloHitList->begin(), hitIterEnd = pCaloHitList->end(); hitIter != hitIterEnd; ++hitIter)
    {
        if (!m_caloHitUsageMap.insert(CaloHitUsageMap::value_type(*hitIter, initialHitAvailability)).second)
            throw StatusCodeException(STATUS_CODE_ALREADY_PRESENT);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

CaloHitMetadata::~CaloHitMetadata()
{
    for (CaloHitReplacementList::iterator iter = m_caloHitReplacementList.begin(), iterEnd = m_caloHitReplacementList.end(); iter != iterEnd; ++iter)
    {
        for (CaloHitList::iterator hitIter = (*iter)->m_newCaloHits.begin(), hitIterEnd = (*iter)->m_newCaloHits.end(); hitIter != hitIterEnd; ++hitIter)
            delete *hitIter;

        delete *iter;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <>
bool CaloHitMetadata::IsAvailable(const CaloHit *const pCaloHit) const
{
    CaloHitUsageMap::const_iterator usageMapIter = m_caloHitUsageMap.find(pCaloHit);

    if ((m_caloHitUsageMap.end()) == usageMapIter || !usageMapIter->second)
        return false;

    return true;
}

template <>
bool CaloHitMetadata::IsAvailable(const CaloHitList *const pCaloHitList) const
{
    for (CaloHitList::const_iterator iter = pCaloHitList->begin(), iterEnd = pCaloHitList->end(); iter != iterEnd; ++iter)
    {
        CaloHitUsageMap::const_iterator usageMapIter = m_caloHitUsageMap.find(*iter);

        if ((m_caloHitUsageMap.end()) == usageMapIter || !usageMapIter->second)
            return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <>
StatusCode CaloHitMetadata::SetAvailability(const CaloHit *const pCaloHit, bool isAvailable)
{
    CaloHitUsageMap::iterator usageMapIter = m_caloHitUsageMap.find(pCaloHit);

    if (m_caloHitUsageMap.end() == usageMapIter)
        return STATUS_CODE_NOT_FOUND;

    usageMapIter->second = isAvailable;

    return STATUS_CODE_SUCCESS;
}

template <>
StatusCode CaloHitMetadata::SetAvailability(const CaloHitList *const pCaloHitList, bool isAvailable)
{
    for (CaloHitList::const_iterator iter = pCaloHitList->begin(), iterEnd = pCaloHitList->end(); iter != iterEnd; ++iter)
    {
        CaloHitUsageMap::iterator usageMapIter = m_caloHitUsageMap.find(*iter);

        if (m_caloHitUsageMap.end() == usageMapIter)
            return STATUS_CODE_NOT_FOUND;

        usageMapIter->second = isAvailable;
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode CaloHitMetadata::Update(const CaloHitMetadata &caloHitMetadata)
{
    const CaloHitReplacementList &caloHitReplacementList(caloHitMetadata.GetCaloHitReplacementList());

    for (CaloHitReplacementList::const_iterator iter = caloHitReplacementList.begin(), iterEnd = caloHitReplacementList.end(); iter != iterEnd; ++iter)
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->Update(*(*iter)));
    }

    const CaloHitUsageMap &caloHitUsageMap(caloHitMetadata.GetCaloHitUsageMap());

    for (CaloHitUsageMap::const_iterator iter = caloHitUsageMap.begin(), iterEnd = caloHitUsageMap.end(); iter != iterEnd; ++iter)
    {
        CaloHitUsageMap::iterator usageMapIter = m_caloHitUsageMap.find(iter->first);

        if (m_caloHitUsageMap.end() == usageMapIter)
            return STATUS_CODE_FAILURE;

        usageMapIter->second = iter->second;
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode CaloHitMetadata::Update(const CaloHitReplacement &caloHitReplacement)
{
    for (CaloHitList::const_iterator iter = caloHitReplacement.m_newCaloHits.begin(), iterEnd = caloHitReplacement.m_newCaloHits.end();
        iter != iterEnd; ++iter)
    {
        if (!m_pCaloHitList->insert(*iter).second)
            return STATUS_CODE_ALREADY_PRESENT;

        if (!m_caloHitUsageMap.insert(CaloHitUsageMap::value_type(*iter, true)).second)
            return STATUS_CODE_ALREADY_PRESENT;
    }

    for (CaloHitList::const_iterator iter = caloHitReplacement.m_oldCaloHits.begin(), iterEnd = caloHitReplacement.m_oldCaloHits.end();
        iter != iterEnd; ++iter)
    {
        CaloHitList::iterator listIter = m_pCaloHitList->find(*iter);

        if (m_pCaloHitList->end() == listIter)
            return STATUS_CODE_FAILURE;

        m_pCaloHitList->erase(listIter);

        CaloHitUsageMap::iterator mapIter = m_caloHitUsageMap.find(*iter);

        if (m_caloHitUsageMap.end() == mapIter)
            return STATUS_CODE_FAILURE;

        m_caloHitUsageMap.erase(mapIter);
    }

    m_caloHitReplacementList.push_back(new CaloHitReplacement(caloHitReplacement));

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void CaloHitMetadata::Clear()
{
    for (CaloHitReplacementList::iterator iter = m_caloHitReplacementList.begin(), iterEnd = m_caloHitReplacementList.end(); iter != iterEnd; ++iter)
        delete *iter;

    m_pCaloHitList = NULL;
    m_caloHitListName.clear();
    m_caloHitUsageMap.clear();
    m_caloHitReplacementList.clear();
}

//------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------

ReclusterMetadata::ReclusterMetadata(CaloHitList *const pCaloHitList) :
    m_pCurrentCaloHitMetadata(NULL),
    m_caloHitList(*pCaloHitList)
{
    if (m_caloHitList.empty())
        throw StatusCodeException(STATUS_CODE_NOT_INITIALIZED);
}

//------------------------------------------------------------------------------------------------------------------------------------------

ReclusterMetadata::~ReclusterMetadata()
{
    for (NameToMetadataMap::iterator iter = m_nameToMetadataMap.begin(), iterEnd = m_nameToMetadataMap.end(); iter != iterEnd; ++iter)
        delete iter->second;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode ReclusterMetadata::CreateCaloHitMetadata(CaloHitList *const pCaloHitList, const std::string &caloHitListName,
    const std::string &reclusterListName, const bool initialHitAvailability)
{
    m_pCurrentCaloHitMetadata = new CaloHitMetadata(pCaloHitList, caloHitListName, initialHitAvailability);

    if (!m_nameToMetadataMap.insert(NameToMetadataMap::value_type(reclusterListName, m_pCurrentCaloHitMetadata)).second)
    {
        delete m_pCurrentCaloHitMetadata;
        return STATUS_CODE_ALREADY_PRESENT;
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode ReclusterMetadata::ExtractCaloHitMetadata(const std::string &reclusterListName, CaloHitMetadata *&pCaloHitMetaData)
{
    NameToMetadataMap::iterator iter = m_nameToMetadataMap.find(reclusterListName);

    if (m_nameToMetadataMap.end() == iter)
        return STATUS_CODE_FAILURE;

    pCaloHitMetaData = iter->second;
    m_nameToMetadataMap.erase(iter);

    return STATUS_CODE_SUCCESS;
}

} // namespace pandora
