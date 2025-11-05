#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <bits/stdc++.h>
#include <chrono>

#include "face/face-endpoint.hpp"

#include "cbs_netlink.hpp"
#include "qdisc_info.hpp"

#include "RSJparser.hpp"


namespace nfd {

struct reservationValues {
  uint64_t Reservation;
  Name interestName;
};

class ReservationTable
{
public:
  explicit
  ReservationTable();

  void
  addReservationFaceMap(const Interest& interest, const FaceEndpoint& ingress);

  void
  addReservation(const Interest& interest, const FaceEndpoint& egress);

  void
  changeQdiscWithTimer();

private:

  void
  readConfigJSON(std::string file);

  void
  addReservationToMap(const Interest& interest, const std::string interface);

private:
  std::chrono::time_point<std::chrono::steady_clock> m_lastQdiscChange;

  // Interval in ms between qdisc changes
  int32_t m_qdiscChangeInterval;

  // map interfaces to interest names 
  std::map< std::string, std::set<std::string> > m_duplicateCheckMap;
  // map interfaces to map of traffic class to accumulated reserved bandwidth
  std::unordered_map< std::string, std::map<uint8_t, uint32_t> > m_reservationMap;

  // vector of traffic classes in descending order that have a CBS qdisc installed
  std::vector<uint8_t> m_cbsTrafficClasses;

  // Maps the incoming interest interfaces to interfaces where data is sent out
  // -> probably not needed
  std::unordered_map<std::string, std::string> m_interfaceMap;

  // Maps EnGINE flows to their underlying interfaces
  std::unordered_map<std::string, std::string> m_flowMap;

  // Map the packet priorities to MQPRIO/TAPRIO traffic classes of CBS qdiscs 
  // if priority is not present, then qdisc is not CBS!
  std::unordered_map< uint8_t, uint8_t > m_priorityTrafficClassMap;

  // These structs save information to calculate the CBS parameters using the cbs_netlink program
  srInfo m_baselineSRConfig;
  genInfo m_baselineGenInfo;

  // Amount of data in reserved packets (in bytes)
  int32_t m_dataMaxFrameSize;
};

} // namespace nfd