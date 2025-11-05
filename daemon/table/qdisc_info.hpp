/*
 * This program is a C++ of a Python script found in EnGINE
 *   -> script at engine/support_scripts/configGenerators/prep_cbs_config.py
 */

#include <iostream>
#include <cstdint>
#include <cmath>
#include <vector>

struct srInfo {
  int32_t maxFrameSize;     // frame size in bytes
  int32_t assignedBitrate;  // assigned bandwidth in kilobits per second
};

struct genInfo {
  int32_t maxFrameSize;     // best effort frame size in bytes
  int32_t portTransmitRate; // link capacity in kilobits per second
};

struct cbsConfigs {
  int32_t idleSlope;
  int32_t sendSlope;
  int32_t hiCredit;
  int32_t loCredit;
};

std::vector<cbsConfigs> prepareCBSInfo (std::vector<srInfo> srInfoVector, genInfo genInfoStruct);
int32_t calculateIdleSlope (double bwfrac, int32_t portTransmitRate);
int32_t calculateSendSlope (double bwfrac, int32_t portTransmitRate);
int32_t calculateLoCredit (int32_t maxFrameSize, double bwfrac, int32_t portTransmitRate);
int32_t calculateHiCredit (std::vector<srInfo> srInfoVector, genInfo genInfoStruct, std::vector<cbsConfigs> cbsConfigsVector, size_t entryIndex);
