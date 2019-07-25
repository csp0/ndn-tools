/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2019,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of ndn-tools (Named Data Networking Essential Tools).
 * See AUTHORS.md for complete list of ndn-tools authors and contributors.
 *
 * ndn-tools is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndn-tools is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndn-tools, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Jerald Paul Abraham <jeraldabraham@email.arizona.edu>
 */

#include "ndnpoke.hpp"

#include <ndn-cxx/encoding/buffer-stream.hpp>

namespace ndn {
namespace peek {

NdnPoke::NdnPoke(Face& face, KeyChain& keyChain, std::istream& input, const PokeOptions& options)
  : m_options(options)
  , m_face(face)
  , m_keyChain(keyChain)
  , m_input(input)
  , m_scheduler(m_face.getIoService())
{
}

void
NdnPoke::start()
{
  auto data = createData();

  if (m_options.wantForceData) {
    m_face.put(*data);
    m_result = Result::DATA_SENT;
    return;
  }

  m_registeredPrefix = m_face.setInterestFilter(m_options.name,
    [this, data] (auto&&...) {
      m_timeoutEvent.cancel();
      m_face.put(*data);
      m_result = Result::DATA_SENT;
      m_registeredPrefix.cancel();
    },
    [this] (auto&&) {
      if (m_options.timeout) {
        m_timeoutEvent = m_scheduler.schedule(*m_options.timeout, [this] {
          m_result = Result::TIMEOUT;
          m_registeredPrefix.cancel();
        });
      }
    },
    [this] (auto&&, const auto& reason) {
      m_result = Result::PREFIX_REG_FAIL;
      std::cerr << "Prefix registration failure (" << reason << ")\n";
    });
}

shared_ptr<Data>
NdnPoke::createData() const
{
  auto data = make_shared<Data>(m_options.name);
  if (m_options.freshnessPeriod) {
    data->setFreshnessPeriod(*m_options.freshnessPeriod);
  }
  if (m_options.wantFinalBlockId) {
    data->setFinalBlock(m_options.name.at(-1));
  }

  OBufferStream os;
  os << m_input.rdbuf();
  data->setContent(os.buf());

  m_keyChain.sign(*data, m_options.signingInfo);

  return data;
}

} // namespace peek
} // namespace ndn
