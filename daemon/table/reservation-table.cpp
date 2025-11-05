#include "reservation-table.hpp"

namespace nfd {

ReservationTable::ReservationTable()
{
  m_duplicateCheckMap = {};
  m_reservationMap = {};

  m_qdiscChangeInterval = 1000;

  m_cbsTrafficClasses = {};
  m_interfaceMap = {};
  m_priorityTrafficClassMap = {};
  m_baselineSRConfig = {};
  m_baselineGenInfo = {};
  m_dataMaxFrameSize = 0;
  readConfigJSON("/root/nfd_reservation_table_config.json");

  m_lastQdiscChange = std::chrono::steady_clock::now();
}

void
ReservationTable::addReservationToMap(const Interest& interest, const std::string interface)
{
  uint8_t priority = (uint8_t) std::stoi(interest.getName().getSubName(1, 1).toUri().substr(5));
  if (m_priorityTrafficClassMap.find(priority) == m_priorityTrafficClassMap.end()) 
    return;

  if (m_duplicateCheckMap.find(interface) == m_duplicateCheckMap.end()) { // interface not found -> add new set/map to maps
    // add set to check for future duplicate interests
    std::set<std::string> interestNameSet = {};
    interestNameSet.insert(interest.getName().getPrefix(-1).toUri());
    m_duplicateCheckMap.insert({interface, interestNameSet});
        
    // add map to count reserved bandwidth
    uint8_t trafficClass = m_priorityTrafficClassMap.at(priority);
    std::map<uint8_t, uint32_t> tcReservationMap = {};
    tcReservationMap.insert({trafficClass, interest.getReservation().value()});
    m_reservationMap.insert({interface, tcReservationMap});
  }
  else { // interface found -> check if reservation is no duplicate -> if not add reservation
    std::set<std::string> interestNameSet = m_duplicateCheckMap.at(interface);
    if (interestNameSet.find(interest.getName().getPrefix(-1).toUri()) == interestNameSet.end()) { // no duplicate -> add reservation
      m_duplicateCheckMap.at(interface).insert(interest.getName().getPrefix(-1).toUri());
          
      uint8_t trafficClass = m_priorityTrafficClassMap.at(priority);
      if (m_reservationMap.at(interface).find(trafficClass) == m_reservationMap.at(interface).end())
        m_reservationMap.at(interface).insert({trafficClass, 0});
      m_reservationMap.at(interface).at(trafficClass) += interest.getReservation().value();
    }
  }
}

// Reserve bandwidth for an interface potentially different to ingress -> saved in interfaceMap map 
// -> normally should not happen in NDN though -> Data should be sent out the ingress interface
void
ReservationTable::addReservationFaceMap(const Interest& interest, const FaceEndpoint& ingress)
{
  if (!interest.hasReservation() || ingress.face.getLocalUri().getScheme().compare("dev") != 0) 
    return;

  const std::string flow = ingress.face.getLocalUri().getHost();
  if (m_flowMap.find(flow) == m_flowMap.end())
    return;

  const std::string ingressInterface = m_flowMap.at(flow); 
  if (m_interfaceMap.find(ingressInterface) == m_interfaceMap.end())
    return;

  const std::string dataInterface = m_interfaceMap.at(ingressInterface);

  addReservationToMap(interest, dataInterface);
}

// Reserve bandwidth for given interface
void
ReservationTable::addReservation(const Interest& interest, const FaceEndpoint& ingress)
{
  if (!interest.hasReservation() || ingress.face.getLocalUri().getScheme().compare("dev") != 0) 
    return;

  const std::string flow = ingress.face.getLocalUri().getHost();
  if (m_flowMap.find(flow) == m_flowMap.end())
    return;

  const std::string dataInterface = m_flowMap.at(flow);

  addReservationToMap(interest, dataInterface);
}

void
ReservationTable::changeQdiscWithTimer()
{
  if (m_interfaceMap.empty() || m_priorityTrafficClassMap.empty())
    return;

  auto nowTime = std::chrono::steady_clock::now();
  if ( (std::chrono::duration<double, std::milli>(nowTime - m_lastQdiscChange).count()) < m_qdiscChangeInterval) 
    return;

  for (std::map< std::string, std::set<std::string> >::const_iterator dev = m_duplicateCheckMap.begin(); dev != m_duplicateCheckMap.end(); ++dev) {
    std::vector<srInfo> srInfoVector = {};
    for (std::vector<uint8_t>::const_iterator tc = m_cbsTrafficClasses.begin(); tc != m_cbsTrafficClasses.end(); ++tc) { // get reservations for each traffic class
      srInfo srInfoStruct = {};
      srInfoStruct.maxFrameSize = m_baselineSRConfig.maxFrameSize;
      if (m_reservationMap.at(dev->first).find(*tc) != m_reservationMap.at(dev->first).end()) { // reservations exist
        srInfoStruct.assignedBitrate = m_baselineSRConfig.assignedBitrate + m_reservationMap.at(dev->first).at(*tc);
        if (m_dataMaxFrameSize > m_baselineSRConfig.maxFrameSize)
          srInfoStruct.maxFrameSize = m_dataMaxFrameSize;
        m_reservationMap.at(dev->first).at(*tc) = 0; // reset reservations
      } 
      else  // no reservations -> only use baseline config
        srInfoStruct.assignedBitrate = m_baselineSRConfig.assignedBitrate;
      srInfoVector.push_back(srInfoStruct);
    }
      
    std::vector<cbsConfigs> cbsConfigsVector = prepareCBSInfo(srInfoVector, m_baselineGenInfo);
    for (size_t i = 0; i < m_cbsTrafficClasses.size(); ++i) { // change CBS parameters for each traffic class
      std::string parentClassID = "100:" + std::to_string(m_cbsTrafficClasses.at(i));
      int qdiscError = change_cbs(dev->first.c_str(), "none", parentClassID.c_str(), cbsConfigsVector.at(i).hiCredit, 
          cbsConfigsVector.at(i).loCredit, cbsConfigsVector.at(i).idleSlope, cbsConfigsVector.at(i).sendSlope);
      if (qdiscError)
        std::cerr << "Error with changing qdisc!" << std::endl;
    }
    m_duplicateCheckMap.at(dev->first).clear(); // reset duplicate checks 
  }
  m_lastQdiscChange = std::chrono::steady_clock::now();
}

// inspired by https://stackoverflow.com/questions/5891610/how-to-remove-certain-characters-from-a-string-in-c
// removes every instance of the character c from str
void 
removeCharFromString (std::string &str, char c) {
    str.erase( std::remove(str.begin(), str.end(), c), str.end() );
}

/* 
 * initializes the variables m_cbsTrafficClasses, m_interfaceMap, m_priorityTrafficClassMap, m_baselineSRConfig and m_baselineGenInfo
 * with values specified in a JSON file at 'file'
 */ 
void
ReservationTable::readConfigJSON(std::string file)
{
  std::ifstream jsonStream (file);
  RSJresource jsonRes (jsonStream);

  m_qdiscChangeInterval = jsonRes["qdiscChangeInterval"].as<int>();

  RSJarray tcArray = jsonRes["cbsTrafficClasses"].as_array();
  for (auto tc = tcArray.begin(); tc != tcArray.end(); ++tc) 
    m_cbsTrafficClasses.push_back((uint8_t) (tc->as<int>()));

  for (int i = 0; i < jsonRes["interfaceMap"]["interest"].size(); ++i) {
    std::string ingress = jsonRes["interfaceMap"]["interest"][i].as_str();
    removeCharFromString(ingress, '\"');
    std::string egress = jsonRes["interfaceMap"]["data"][i].as_str();
    removeCharFromString(egress, '\"');
    m_interfaceMap.insert({ingress, egress});
  }

  for (int i = 0; i < jsonRes["flowMap"]["flows"].size(); ++i) {
    std::string flow = jsonRes["flowMap"]["flows"][i].as_str();
    removeCharFromString(flow, '\"');
    std::string interface = jsonRes["flowMap"]["interfaces"][i].as_str();
    removeCharFromString(interface, '\"');
    m_flowMap.insert({flow, interface});
  }

  for (int i = 0; i < jsonRes["priorityTrafficClassMap"]["priorities"].size(); ++i) {
    uint8_t prio = jsonRes["priorityTrafficClassMap"]["priorities"][i].as<int>();
    uint8_t tc = jsonRes["priorityTrafficClassMap"]["trafficClasses"][i].as<int>();
    m_priorityTrafficClassMap.insert({prio, tc});
  }

  m_dataMaxFrameSize = jsonRes["dataMaxFrameSize"].as<int>();

  m_baselineSRConfig.maxFrameSize = jsonRes["baselineSRConfig"]["maxFrameSize"].as<int>();
  m_baselineSRConfig.assignedBitrate = jsonRes["baselineSRConfig"]["assignedBitrate"].as<int>();
    
  m_baselineGenInfo.maxFrameSize = jsonRes["baselineGenInfo"]["maxFrameSize"].as<int>();
  m_baselineGenInfo.portTransmitRate = jsonRes["baselineGenInfo"]["portTransmitRate"].as<int>();
}

}