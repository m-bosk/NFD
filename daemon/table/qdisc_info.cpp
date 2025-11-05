#include "qdisc_info.hpp"

int32_t 
calculateIdleSlope (double bwfrac, int32_t portTransmitRate)
{
    return std::ceil(bwfrac * (double) portTransmitRate);
}

int32_t 
calculateSendSlope (double bwfrac, int32_t portTransmitRate)
{
    return (int32_t) std::floor(calculateIdleSlope(bwfrac, portTransmitRate) - portTransmitRate);
}

int32_t 
calculateLoCredit (int32_t maxFrameSize, double bwfrac, int32_t portTransmitRate)
{
    return std::floor(maxFrameSize * ( (double) calculateSendSlope(bwfrac, portTransmitRate) / portTransmitRate ));
}

int32_t 
calculateHiCredit (std::vector<srInfo> srInfoVector, genInfo genInfoStruct, std::vector<cbsConfigs> cbsConfigsVector, size_t entryIndex)
{
    double hicredit {0.0};
    int32_t index = entryIndex;
    while (index > 0) {
        int32_t prevIndex = index - 1;
        cbsConfigs prevCBSConfig = cbsConfigsVector.at(prevIndex);
        hicredit += ( (double) prevCBSConfig.hiCredit / (-prevCBSConfig.sendSlope) ) + ( (double) srInfoVector.at(prevIndex).maxFrameSize / genInfoStruct.portTransmitRate );
        --index;
    }
    hicredit += (double) genInfoStruct.maxFrameSize / genInfoStruct.portTransmitRate;
    hicredit *= (double) cbsConfigsVector.at(entryIndex).idleSlope;
    return std::ceil(hicredit);
}

std::vector<cbsConfigs> 
prepareCBSInfo (std::vector<srInfo> srInfoVector, genInfo genInfoStruct)
{
    std::vector<cbsConfigs> cbsConfigsVector = {};
    for (size_t i = 0; i < srInfoVector.size(); ++i) {
        double bwfrac = srInfoVector.at(i).assignedBitrate / (double) genInfoStruct.portTransmitRate;

        cbsConfigs cbsConfigsStruct = {};
        cbsConfigsVector.push_back(cbsConfigsStruct);

        cbsConfigsVector.at(i).idleSlope = calculateIdleSlope(bwfrac, genInfoStruct.portTransmitRate);      
        cbsConfigsVector.at(i).sendSlope = calculateSendSlope(bwfrac, genInfoStruct.portTransmitRate);       
        cbsConfigsVector.at(i).loCredit = calculateLoCredit(srInfoVector.at(i).maxFrameSize, bwfrac, genInfoStruct.portTransmitRate);
        cbsConfigsVector.at(i).hiCredit = calculateHiCredit(srInfoVector, genInfoStruct, cbsConfigsVector, i);
    }
    return cbsConfigsVector;
}
