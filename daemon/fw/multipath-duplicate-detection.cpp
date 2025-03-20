/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2024,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "multipath-duplicate-detection.hpp"
#include "common/logger.hpp"

namespace nfd {
    NFD_LOG_INIT(MpathDuplDetect);

    void MultipathDuplicateDetection::init(size_t size) {
        capacity = size;
    }

    void MultipathDuplicateDetection::push(ndn::Block val) {
        NFD_LOG_DEBUG("Pushing into duplicate detection data structure with capacity of " << capacity << " now filled with " << qe.size() << " packets.");
        if (capacity <= 0) return;
        if (exists(val)) return;
        std::string strVal = std::string(val.begin(), val.end());
        
        if (qe.size() == capacity) {
            std::string popped = qe.front();
            qe.pop();
            st.erase(popped);
        }
        qe.push(strVal);
        st.insert(strVal);
    }

    bool MultipathDuplicateDetection::exists(ndn::Block val) const {
        NFD_LOG_DEBUG("Checking if packet in duplicate detection data structure with capacity of " << capacity << " now filled with " << qe.size() << " packets.");
        if (capacity > 0) {
            std::string strVal = std::string(val.begin(), val.end());
            return st.find(strVal) != st.end();
        }
        return false;
    }

} // namespace nfd::fw
