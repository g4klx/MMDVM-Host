/*
 *	Copyright (C) 2015-2019,2021,2023,2025,2026 Jonathan Naylor, G4KLX
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 */

#include "DStarControl.h"
#include "Utils.h"
#include "Sync.h"
#include "Log.h"

#if defined(USE_DSTAR)

#include <cstdio>
#include <cassert>
#include <ctime>
#include <algorithm>
#include <functional>

const unsigned int MAX_SYNC_BIT_ERRORS = 2U;

const unsigned int RSSI_COUNT = 3U * 21U;	// 3 * 420ms = 1260ms
const unsigned int BER_COUNT  = 50U * 48U;	// 50 * 20ms = 1000ms

bool CallsignCompare(const std::string& arg, const unsigned char* my)
{
	for (unsigned int i = 0U; i < (DSTAR_LONG_CALLSIGN_LENGTH - 1U); i++) {
		if (arg.at(i) != my[i])
			return false;
	}

	return true;
}

CDStarControl::CDStarControl(const std::string& callsign, const std::string& module, bool selfOnly, bool ackReply, unsigned int ackTime, DSTAR_ACK ackMessage, bool errorReply, const std::vector<std::string>& blackList, const std::vector<std::string>& whiteList, CDStarNetwork* network, unsigned int timeout, bool duplex, bool remoteGateway, CRSSIInterpolator* rssiMapper) :
m_callsign(nullptr),
m_gateway(nullptr),
m_selfOnly(selfOnly),
m_ackReply(ackReply),
m_ackMessage(ackMessage),
m_errorReply(errorReply),
m_remoteGateway(remoteGateway),
m_blackList(blackList),
m_whiteList(whiteList),
m_network(network),
m_duplex(duplex),
m_queue(5000U, "D-Star Control"),
m_rfHeader(),
m_netHeader(),
m_rfState(RPT_RF_STATE::LISTENING),
m_netState(RPT_NET_STATE::IDLE),
m_net(false),
m_rfSlowData(),
m_netSlowData(),
m_rfN(0U),
m_netN(0U),
m_networkWatchdog(1000U, 0U, 1500U),
m_rfTimeoutTimer(1000U, timeout),
m_netTimeoutTimer(1000U, timeout),
m_packetTimer(1000U, 0U, 300U),
m_ackTimer(1000U, 0U, ackTime),
m_errTimer(1000U, 0U, ackTime),
m_interval(),
m_elapsed(),
m_rfFrames(0U),
m_netFrames(0U),
m_netLost(0U),
m_fec(),
m_rfBits(1U),
m_netBits(1U),
m_rfErrs(0U),
m_lastFrame(nullptr),
m_lastFrameValid(false),
m_rssiMapper(rssiMapper),
m_rssi(0),
m_maxRSSI(0),
m_minRSSI(0),
m_aveRSSI(0),
m_rssiCountTotal(0U),
m_rssiAccum(0),
m_rssiCount(0U),
m_bitErrsAccum(0U),
m_bitsCount(0U),
m_enabled(true),
m_rfDataLookBack(nullptr),
m_rfDataLookBackLen(0U),
m_rfDataLookBackIndex(0U),
m_rfDataLookBackReplay(false),
m_netDataLookBack(nullptr),
m_netDataLookBackLen(0U),
m_netDataLookBackIndex(0U)
{
	assert(rssiMapper != nullptr);

	m_callsign = new unsigned char[DSTAR_LONG_CALLSIGN_LENGTH];
	m_gateway  = new unsigned char[DSTAR_LONG_CALLSIGN_LENGTH];

	m_lastFrame = new unsigned char[DSTAR_FRAME_LENGTH_BYTES + 1U];

	std::string call = callsign;
	call.resize(DSTAR_LONG_CALLSIGN_LENGTH - 1U, ' ');
	std::string mod = module;
	mod.resize(1U, ' ');
	call.append(mod);

	std::string gate = callsign;
	gate.resize(DSTAR_LONG_CALLSIGN_LENGTH - 1U, ' ');
	gate.append("G");

	for (unsigned int i = 0U; i < DSTAR_LONG_CALLSIGN_LENGTH; i++) {
		m_callsign[i] = call.at(i);
		m_gateway[i]  = gate.at(i);
	}

	m_interval.start();
}

CDStarControl::~CDStarControl()
{
	delete[] m_callsign;
	delete[] m_gateway;
	delete[] m_lastFrame;
}

bool CDStarControl::writeModem(unsigned char *data, unsigned int len)
{
	assert(data != nullptr);

	if (!m_enabled)
		return false;

	unsigned char type = data[0U];

	if (type == TAG_LOST && ((m_rfState == RPT_RF_STATE::AUDIO) || (m_rfState == RPT_RF_STATE::DATA))) {
		unsigned char my1[DSTAR_LONG_CALLSIGN_LENGTH];
		unsigned char my2[DSTAR_SHORT_CALLSIGN_LENGTH];
		unsigned char your[DSTAR_LONG_CALLSIGN_LENGTH];
		m_rfHeader.getMyCall1(my1);
		m_rfHeader.getMyCall2(my2);
		m_rfHeader.getYourCall(your);

		if (m_rssi != 0) {
			LogMessage("D-Star, transmission lost from %8.8s/%4.4s to %8.8s, %.1f seconds, BER: %.1f%%, RSSI: %d/%d/%d dBm", my1, my2, your, float(m_rfFrames) / 50.0F, float(m_rfErrs * 100U) / float(m_rfBits), m_minRSSI, m_maxRSSI, m_aveRSSI / int(m_rssiCountTotal));
			writeJSONRF("lost", float(m_rfFrames) / 50.0F, float(m_rfErrs * 100U) / float(m_rfBits), m_minRSSI, m_maxRSSI, m_aveRSSI / int(m_rssiCountTotal));
		} else {
			LogMessage("D-Star, transmission lost from %8.8s/%4.4s to %8.8s, %.1f seconds, BER: %.1f%%", my1, my2, your, float(m_rfFrames) / 50.0F, float(m_rfErrs * 100U) / float(m_rfBits));
			writeJSONRF("lost", float(m_rfFrames) / 50.0F, float(m_rfErrs * 100U) / float(m_rfBits));
		}
		writeEndRF();
		return false;
	}

	if ((type == TAG_LOST) && (m_rfState == RPT_RF_STATE::INVALID)) {
		m_rfState = RPT_RF_STATE::LISTENING;

		if (m_netState == RPT_NET_STATE::IDLE) {
			if (m_errorReply)
				m_errTimer.start();

			if (m_network != nullptr)
				m_network->reset();
		}

		return false;
	}

	if (type == TAG_LOST) {
		m_rfState = RPT_RF_STATE::LISTENING;
		return false;
	}

	// Have we got RSSI bytes on the end of a D-Star header?
	if (len == (DSTAR_HEADER_LENGTH_BYTES + 3U)) {
		uint16_t raw = 0U;
		raw |= (data[42U] << 8) & 0xFF00U;
		raw |= (data[43U] << 0) & 0x00FFU;

		// Convert the raw RSSI to dBm
		m_rssi = m_rssiMapper->interpolate(raw);
		if (m_rssi != 0)
			LogDebug("D-Star, raw RSSI: %u, reported RSSI: %d dBm", raw, m_rssi);

		if (m_rssi < m_minRSSI)
			m_minRSSI = m_rssi;
		if (m_rssi > m_maxRSSI)
			m_maxRSSI = m_rssi;

		m_aveRSSI += m_rssi;
		m_rssiCountTotal++;

		m_rssiAccum += m_rssi;
	}

	// Have we got RSSI bytes on the end of D-Star data?
	if (len == (DSTAR_FRAME_LENGTH_BYTES + 3U)) {
		uint16_t raw = 0U;
		raw |= (data[13U] << 8) & 0xFF00U;
		raw |= (data[14U] << 0) & 0x00FFU;

		// Convert the raw RSSI to dBm
		m_rssi = m_rssiMapper->interpolate(raw);
		if (m_rssi != 0)
			LogDebug("D-Star, raw RSSI: %u, reported RSSI: %d dBm", raw, m_rssi);

		if (m_rssi < m_minRSSI)
			m_minRSSI = m_rssi;
		if (m_rssi > m_maxRSSI)
			m_maxRSSI = m_rssi;

		m_aveRSSI += m_rssi;
		m_rssiCountTotal++;

		m_rssiAccum += m_rssi;
	}

	if (type == TAG_HEADER) {
		CDStarHeader header(data + 1U);
		m_rfHeader = header;

		unsigned char my1[DSTAR_LONG_CALLSIGN_LENGTH];
		header.getMyCall1(my1);

		unsigned char gateway[DSTAR_LONG_CALLSIGN_LENGTH];
		header.getRPTCall2(gateway);

		unsigned char my2[DSTAR_SHORT_CALLSIGN_LENGTH];
		header.getMyCall2(my2);

		unsigned char your[DSTAR_LONG_CALLSIGN_LENGTH];
		header.getYourCall(your);

		unsigned char rpt1[DSTAR_LONG_CALLSIGN_LENGTH];
		header.getRPTCall1(rpt1);

		// Is this a transmission destined for a repeater?
		if (!header.isRepeater()) {
			LogMessage("D-Star, non repeater RF header received from %8.8s", my1);
			m_rfState = RPT_RF_STATE::INVALID;
			writeJSONRF("invalid", my1, my2, your);
			return true;
		}

		// Is it for us?
		if (::memcmp(rpt1, m_callsign, DSTAR_LONG_CALLSIGN_LENGTH) != 0) {
			LogMessage("D-Star, received RF header for wrong repeater (%8.8s) from %8.8s", rpt1, my1);
			m_rfState = RPT_RF_STATE::INVALID;
			writeJSONRF("invalid", my1, my2, your);
			return true;
		}

		if (m_selfOnly && ::memcmp(my1, m_callsign, DSTAR_LONG_CALLSIGN_LENGTH - 1U) != 0 && !(std::find_if(m_whiteList.begin(), m_whiteList.end(), std::bind(CallsignCompare, std::placeholders::_1, my1)) != m_whiteList.end())) {
			LogMessage("D-Star, invalid access attempt from %8.8s", my1);
			m_rfState = RPT_RF_STATE::REJECTED;
			writeJSONRF("rejected", my1, my2, your);
			return true;
		}

		if (!m_selfOnly && std::find_if(m_blackList.begin(), m_blackList.end(), std::bind(CallsignCompare, std::placeholders::_1, my1)) != m_blackList.end()) {
			LogMessage("D-Star, invalid access attempt from %8.8s", my1);
			m_rfState = RPT_RF_STATE::REJECTED;
			writeJSONRF("rejected", my1, my2, your);
			return true;
		}

		m_net = ::memcmp(gateway, m_gateway, DSTAR_LONG_CALLSIGN_LENGTH) == 0;

		// Only start the timeout if not already running
		if (!m_rfTimeoutTimer.isRunning())
			m_rfTimeoutTimer.start();

		m_ackTimer.stop();
		m_errTimer.stop();

		m_rfBits = 1U;
		m_rfErrs = 0U;

		m_rfFrames = 1U;
		m_rfN = 0U;
		m_rfDataLookBackIndex = 0U;

		m_minRSSI = m_rssi;
		m_maxRSSI = m_rssi;
		m_aveRSSI = m_rssi;
		m_rssiCountTotal = 1U;

		m_rssiAccum = m_rssi;
		m_rssiCount = 1U;

		m_rfSlowData.start();

		if (m_duplex) {
			// Modify the header
			header.setRepeater(false);
			header.setRPTCall1(m_callsign);
			header.setRPTCall2(m_callsign);
			header.get(data + 1U);

			writeQueueHeaderRF(data);
		}

		if (m_net) {
			// Modify the header
			header.setRepeater(false);
			header.setRPTCall1(m_callsign);
			header.setRPTCall2(m_gateway);
			header.get(data + 1U);

			writeNetworkHeaderRF(data);
		}

		m_rfState = RPT_RF_STATE::AUDIO;

		if (m_netState == RPT_NET_STATE::IDLE)
			writeJSONRSSI();

		LogMessage("D-Star, received RF header from %8.8s/%4.4s to %8.8s", my1, my2, your);
		writeJSONRF("start", my1, my2, your);
	} else if (type == TAG_EOT) {
		if (m_rfState == RPT_RF_STATE::REJECTED) {
			m_rfState = RPT_RF_STATE::LISTENING;
		} else if (m_rfState == RPT_RF_STATE::INVALID) {
			m_rfState = RPT_RF_STATE::LISTENING;

			if (m_netState == RPT_NET_STATE::IDLE) {
				if (m_errorReply)
					m_errTimer.start();

				if (m_network != nullptr)
					m_network->reset();
			}

			return false;
		} else if ((m_rfState == RPT_RF_STATE::AUDIO) || (m_rfState == RPT_RF_STATE::DATA)) {
			if (m_net)
				writeNetworkDataRF(DSTAR_END_PATTERN_BYTES, 0U, true);

			if (m_duplex)
				writeQueueEOTRF();

			unsigned char my1[DSTAR_LONG_CALLSIGN_LENGTH];
			unsigned char my2[DSTAR_SHORT_CALLSIGN_LENGTH];
			unsigned char your[DSTAR_LONG_CALLSIGN_LENGTH];
			m_rfHeader.getMyCall1(my1);
			m_rfHeader.getMyCall2(my2);
			m_rfHeader.getYourCall(your);

			if (m_rssi != 0) {
				LogMessage("D-Star, received RF end of transmission from %8.8s/%4.4s to %8.8s, %.1f seconds, BER: %.1f%%, RSSI: %d/%d/%d dBm", my1, my2, your, float(m_rfFrames) / 50.0F, float(m_rfErrs * 100U) / float(m_rfBits), m_minRSSI, m_maxRSSI, m_aveRSSI / int(m_rssiCountTotal));
				writeJSONRF("end", float(m_rfFrames) / 50.0F, float(m_rfErrs * 100U) / float(m_rfBits), m_minRSSI, m_maxRSSI, m_aveRSSI / int(m_rssiCountTotal));
			} else {
				LogMessage("D-Star, received RF end of transmission from %8.8s/%4.4s to %8.8s, %.1f seconds, BER: %.1f%%", my1, my2, your, float(m_rfFrames) / 50.0F, float(m_rfErrs * 100U) / float(m_rfBits));
				writeJSONRF("end", float(m_rfFrames) / 50.0F, float(m_rfErrs * 100U) / float(m_rfBits));
			}

			writeEndRF();
		}

		return false;
	} else if (type == TAG_DATA) {
		if (m_rfState == RPT_RF_STATE::REJECTED)
			return true;

		if (m_rfState == RPT_RF_STATE::INVALID)
			return true;

		// Replayed lookback frames have already been through the slow data decoder
		if (!m_rfDataLookBackReplay) {
			// The sync is regenerated by the modem so can do exact match
			if (::memcmp(data + 1U + DSTAR_VOICE_FRAME_LENGTH_BYTES, DSTAR_SYNC_BYTES, DSTAR_DATA_FRAME_LENGTH_BYTES) == 0) {
				m_rfSlowData.start();
				m_rfN = 0U;
			} else {
				m_rfSlowData.add(data + 1U);
			}
		}

		if (m_rfDataLookBackIndex == 0U) {
			m_rfDataLookBack = new unsigned char[len];
			::memcpy(m_rfDataLookBack, data, len);
			m_rfDataLookBackLen = len;
			m_rfDataLookBackIndex ++;
			return true;
		} else if (m_rfDataLookBackIndex == 1U) {
			m_rfDataLookBackIndex ++;

			if (m_rfState == RPT_RF_STATE::AUDIO){
				unsigned char type = m_rfSlowData.getType(false);
				// as per specs chapter 6.1.3, this could be any value bewtween 0x81 and 0x9C as one fast data block can only contain 28 bytes 0x80 + 28d = 0x9C 
				if ((type & DSTAR_SLOW_DATA_TYPE_FASTDATA_MASK) == DSTAR_SLOW_DATA_TYPE_FASTDATA){
					LogMessage("D-Star, starting fast data mode (RF)");
					m_rfState = RPT_RF_STATE::DATA;
				}
			}

			LogDebug("D-Star, processing lookback data (RF)");
			// Replay both frames through the normal path
			m_rfDataLookBackReplay = true;
			bool res1 = writeModem(m_rfDataLookBack, m_rfDataLookBackLen);
			bool res2 = writeModem(data, len);
			m_rfDataLookBackReplay = false;
			delete[] m_rfDataLookBack;
			m_rfDataLookBack = nullptr;
			m_rfDataLookBackLen = 0U;
			return res1 && res2;
		}

		if (m_rfState == RPT_RF_STATE::LISTENING) {
			// The sync is regenerated by the modem so can do exact match
			if (::memcmp(data + 1U + DSTAR_VOICE_FRAME_LENGTH_BYTES, DSTAR_SYNC_BYTES, DSTAR_DATA_FRAME_LENGTH_BYTES) == 0) {
				m_rfSlowData.start();
				m_rfState = RPT_RF_STATE::LATE_ENTRY;
			}

			return false;
		}

		if (m_rfState == RPT_RF_STATE::AUDIO) {
			unsigned char type = m_rfSlowData.getType(false);
			//as per specs, this could be any value bewtween 0x80 and 0x9C
			if ((type & DSTAR_SLOW_DATA_TYPE_FASTDATA_MASK) == DSTAR_SLOW_DATA_TYPE_FASTDATA) {
				LogMessage("D-Star, starting fast data mode (RF)");
				m_rfState = RPT_RF_STATE::DATA;
			}
		}

		if (m_rfState == RPT_RF_STATE::DATA) {
			unsigned char type = m_rfSlowData.getType(true);
			// as per specs, this could be any value bewtween 0x80 and 0x9C as one fast data block can only contain 28 bytes 0x80 + 28d = 0x9C 
			if(((type & DSTAR_SLOW_DATA_TYPE_FASTDATA_MASK) == DSTAR_SLOW_DATA_TYPE_FASTDATA) && type > 0x9CU) {
				LogMessage("Invalid fast data type 0x%02X received, returning to audio mode (RF)", type);
				m_rfState = RPT_RF_STATE::AUDIO;
			}
		}

		if (m_rfState == RPT_RF_STATE::DATA) {
			if (m_rfN == 0U)
				writeJSONRSSI();

			LogDebug("D-Star, RF fast data sequence no. %u", m_rfN);

			m_bitsCount += 48U;
			m_rfBits    += 48U;
			m_rfFrames++;

			if (m_net)
				writeNetworkDataRF(data, 0U, false);

			if (m_duplex)
				writeQueueDataRF(data);

			bool complete = m_rfSlowData.isComplete();
			if (complete) {
				unsigned char type = m_rfSlowData.getType(false);
				if ((type & DSTAR_SLOW_DATA_TYPE_FASTDATA_MASK) != DSTAR_SLOW_DATA_TYPE_FASTDATA) {
					LogMessage("D-Star, leaving fast data mode (RF)");
					m_rfState = RPT_RF_STATE::AUDIO;
				}
			}

			m_rfN = (m_rfN + 1U) % 21U;
		} else if (m_rfState == RPT_RF_STATE::AUDIO) {
			if (m_rfN == 0U)
				writeJSONRSSI();

			unsigned int errors = 0U;
			if (::memcmp(data + 1U, DSTAR_NULL_AMBE_DATA_BYTES_SCRAMBLED, DSTAR_VOICE_FRAME_LENGTH_BYTES) == 0) {
				LogDebug("D-Star, RF audio sequence no. %u, null audio", m_rfN);
			} else {
				errors = m_fec.regenerateDStar(data + 1U);
				LogDebug("D-Star, RF audio sequence no. %u, errs: %u/48 (%.1f%%)", m_rfN, errors, float(errors) / 0.48F);

        m_bitErrsAccum += errors;
				m_rfErrs       += errors;
				writeJSONBER();
			}

			m_bitsCount += 48U;
			m_rfBits    += 48U;
			m_rfFrames++;

			if (m_rfN != 0U) {
				const unsigned char* text = m_rfSlowData.getText();
				if (text != nullptr)
					LogMessage("D-Star, RF slow data text = \"%s\"", text);
			}

			if (m_net)
				writeNetworkDataRF(data, errors, false);

			if (m_duplex) {
				blankDTMF(data + 1U);
				writeQueueDataRF(data);
			}

			m_rfN = (m_rfN + 1U) % 21U;
		}

		if (m_rfState == RPT_RF_STATE::LATE_ENTRY) {
			// The sync is regenerated by the modem so can do exact match
			if (::memcmp(data + 1U + DSTAR_VOICE_FRAME_LENGTH_BYTES, DSTAR_SYNC_BYTES, DSTAR_DATA_FRAME_LENGTH_BYTES) == 0) {
				m_rfSlowData.reset();
				return false;
			} else {
				CDStarHeader* header = m_rfSlowData.getHeader();
				if (header == nullptr)
					return false;

				m_rfHeader = *header;
				delete header;
			}

			unsigned char my1[DSTAR_LONG_CALLSIGN_LENGTH];
			m_rfHeader.getMyCall1(my1);

			unsigned char my2[DSTAR_SHORT_CALLSIGN_LENGTH];
			m_rfHeader.getMyCall2(my2);

			unsigned char your[DSTAR_LONG_CALLSIGN_LENGTH];
			m_rfHeader.getYourCall(your);

			unsigned char rpt1[DSTAR_LONG_CALLSIGN_LENGTH];
			m_rfHeader.getRPTCall1(rpt1);

			// Is this a transmission destined for a repeater?
			if (!m_rfHeader.isRepeater()) {
				LogMessage("D-Star, non repeater RF header received from %8.8s", my1);
				m_rfState = RPT_RF_STATE::INVALID;
				writeJSONRF("invalid", my1, my2, your);
				return true;
			}

			// Is it for us?
			if (::memcmp(rpt1, m_callsign, DSTAR_LONG_CALLSIGN_LENGTH) != 0) {
				LogMessage("D-Star, received RF header for wrong repeater (%8.8s) from %8.8s", rpt1, my1);
				m_rfState = RPT_RF_STATE::INVALID;
				writeJSONRF("invalid", my1, my2, your);
				return true;
			}

			if (m_selfOnly && ::memcmp(my1, m_callsign, DSTAR_LONG_CALLSIGN_LENGTH - 1U) != 0 && !(std::find_if(m_whiteList.begin(), m_whiteList.end(), std::bind(CallsignCompare, std::placeholders::_1, my1)) != m_whiteList.end())) {
				LogMessage("D-Star, invalid access attempt from %8.8s", my1);
				m_rfState = RPT_RF_STATE::REJECTED;
				writeJSONRF("rejected", my1, my2, your);
				return true;
			}

			if (!m_selfOnly && std::find_if(m_blackList.begin(), m_blackList.end(), std::bind(CallsignCompare, std::placeholders::_1, my1)) != m_blackList.end()) {
				LogMessage("D-Star, invalid access attempt from %8.8s", my1);
				m_rfState = RPT_RF_STATE::REJECTED;
				writeJSONRF("rejected", my1, my2, your);
				return true;
			}

			unsigned char gateway[DSTAR_LONG_CALLSIGN_LENGTH];
			m_rfHeader.getRPTCall2(gateway);

			m_net = ::memcmp(gateway, m_gateway, DSTAR_LONG_CALLSIGN_LENGTH) == 0;

			// Only reset the timeout if the timeout is not running
			if (!m_rfTimeoutTimer.isRunning())
				m_rfTimeoutTimer.start();

			// Create a dummy start frame to replace the received frame
			m_ackTimer.stop();
			m_errTimer.stop();

			m_rfBits = 1U;
			m_rfErrs = 0U;

			m_rfN = 0U;
			m_rfFrames = 1U;
			m_rfDataLookBackIndex = 0U;

			m_minRSSI = m_rssi;
			m_maxRSSI = m_rssi;
			m_aveRSSI = m_rssi;
			m_rssiCountTotal = 1U;

			if (m_duplex) {
				unsigned char start[DSTAR_HEADER_LENGTH_BYTES + 1U];
				start[0U] = TAG_HEADER;

				// Modify the header
				CDStarHeader header(m_rfHeader);
				header.setRepeater(false);
				header.setRPTCall1(m_callsign);
				header.setRPTCall2(m_callsign);
				header.get(start + 1U);

				writeQueueHeaderRF(start);
			}

			if (m_net) {
				unsigned char start[DSTAR_HEADER_LENGTH_BYTES + 1U];
				start[0U] = TAG_HEADER;

				// Modify the header
				CDStarHeader header(m_rfHeader);
				header.setRepeater(false);
				header.setRPTCall1(m_callsign);
				header.setRPTCall2(m_gateway);
				header.get(start + 1U);

				writeNetworkHeaderRF(start);
			}

			unsigned int errors = 0U;
			if (::memcmp(data + 1U, DSTAR_NULL_AMBE_DATA_BYTES_SCRAMBLED, DSTAR_VOICE_FRAME_LENGTH_BYTES) == 0) {
				LogDebug("D-Star, audio sequence no. %u, null audio", m_rfN);
			} else {
				errors = m_fec.regenerateDStar(data + 1U);
				LogDebug("D-Star, audio sequence no. %u, errs: %u/48 (%.1f%%)", m_rfN, errors, float(errors) / 0.48F);
				m_bitErrsAccum += errors;
				m_rfErrs += errors;
			}

			m_bitsCount += 48U;
			m_rfBits    += 48U;

			if (m_net)
				writeNetworkDataRF(data, errors, false);

			if (m_duplex) {
				blankDTMF(data + 1U);
				writeQueueDataRF(data);
			}

			m_rfState = RPT_RF_STATE::AUDIO;

			m_rfN = (m_rfN + 1U) % 21U;

			if (m_netState == RPT_NET_STATE::IDLE) {
				writeJSONRSSI();
				writeJSONBER();
			}

			LogMessage("D-Star, received RF late entry from %8.8s/%4.4s to %8.8s", my1, my2, your);
			writeJSONRF("late_entry", my1, my2, your);
		}
	} else {
		CUtils::dump("D-Star, unknown data from modem", data, DSTAR_FRAME_LENGTH_BYTES + 1U);
	}

	return true;
}

unsigned int CDStarControl::readModem(unsigned char* data)
{
	assert(data != nullptr);

	if (m_queue.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_queue.getData(&len, 1U);

	m_queue.getData(data, len);

	return len;
}

void CDStarControl::writeEndRF()
{
	m_rfState = RPT_RF_STATE::LISTENING;

	m_rfTimeoutTimer.stop();

	if (m_netState == RPT_NET_STATE::IDLE) {
		m_ackTimer.start();

		if (m_network != nullptr)
			m_network->reset();
	}
}

void CDStarControl::writeEndNet()
{
	m_netState = RPT_NET_STATE::IDLE;

	m_lastFrameValid = false;

	m_netTimeoutTimer.stop();
	m_networkWatchdog.stop();
	m_packetTimer.stop();

	if (m_network != nullptr)
		m_network->reset();
}

void CDStarControl::writeNetwork()
{
	assert(m_network != nullptr);

	unsigned char data[DSTAR_HEADER_LENGTH_BYTES + 2U];
	unsigned int length = m_network->read(data, DSTAR_HEADER_LENGTH_BYTES + 2U);
	if (length == 0U)
		return;

	if (!m_enabled)
		return;

	if (((m_rfState == RPT_RF_STATE::AUDIO) || (m_rfState == RPT_RF_STATE::DATA)) && (m_netState == RPT_NET_STATE::IDLE))
		return;

	m_networkWatchdog.start();

	unsigned char type = data[0U];

	if (type == TAG_HEADER) {
		if (m_netState != RPT_NET_STATE::IDLE)
			return;

		CDStarHeader header(data + 1U);

		unsigned char my1[DSTAR_LONG_CALLSIGN_LENGTH];
		header.getMyCall1(my1);

		unsigned char my2[DSTAR_SHORT_CALLSIGN_LENGTH];
		header.getMyCall2(my2);

		unsigned char your[DSTAR_LONG_CALLSIGN_LENGTH];
		header.getYourCall(your);

		m_netHeader = header;

		m_netTimeoutTimer.start();
		m_packetTimer.start();
		m_ackTimer.stop();
		m_errTimer.stop();

		m_lastFrameValid = false;

		m_netFrames = 0U;
		m_netLost   = 0U;
		m_netDataLookBackIndex = 0U;

		m_netN      = 20U;

		m_netBits   = 1U;

		if (m_remoteGateway) {
			header.setRepeater(true);
			header.setRPTCall1(m_callsign);
			header.setRPTCall2(m_callsign);
			header.get(data + 1U);
		}

		writeQueueHeaderNet(data);

		m_netState = RPT_NET_STATE::AUDIO;

		LINK_STATUS status = LINK_STATUS::NONE;
		unsigned char reflector[DSTAR_LONG_CALLSIGN_LENGTH];
		m_network->getStatus(status, reflector);
		if ((status == LINK_STATUS::LINKED_DEXTRA) || (status == LINK_STATUS::LINKED_DPLUS) || (status == LINK_STATUS::LINKED_DCS) || (status == LINK_STATUS::LINKED_CCS) || (status == LINK_STATUS::LINKED_LOOPBACK)) {
			LogMessage("D-Star, received network header from %8.8s/%4.4s to %8.8s via %8.8s", my1, my2, your, reflector);
			writeJSONNet("start", my1, my2, your, reflector);
		} else {
			LogMessage("D-Star, received network header from %8.8s/%4.4s to %8.8s", my1, my2, your);
			writeJSONNet("start", my1, my2, your);
		}	

		m_elapsed.start();
	} else if (type == TAG_EOT) {
		if ((m_netState != RPT_NET_STATE::AUDIO) && (m_netState != RPT_NET_STATE::DATA))
			return;

		writeQueueEOTNet();

		data[1U] = TAG_EOT;

		unsigned char my1[DSTAR_LONG_CALLSIGN_LENGTH];
		unsigned char my2[DSTAR_SHORT_CALLSIGN_LENGTH];
		unsigned char your[DSTAR_LONG_CALLSIGN_LENGTH];
		m_netHeader.getMyCall1(my1);
		m_netHeader.getMyCall2(my2);
		m_netHeader.getYourCall(your);

		// We've received the header and EOT haven't we?
		m_netFrames += 2U;
		LogMessage("D-Star, received network end of transmission from %8.8s/%4.4s to %8.8s, %.1f seconds, %u%% packet loss", my1, my2, your, float(m_netFrames) / 50.0F, (m_netLost * 100U) / m_netFrames);
		writeJSONNet("end", float(m_netFrames) / 50.0F, float(m_netLost * 100U) / float(m_netFrames));

		writeEndNet();
	} else if (type == TAG_DATA) {
		writeNetworkData(data, length);
	} else {
		CUtils::dump("D-Star, unknown data from network", data, DSTAR_FRAME_LENGTH_BYTES + 1U);
	}
}

void CDStarControl::writeNetworkData(unsigned char* data, unsigned int length)
{
	assert(data != nullptr);

	unsigned char type = data[0U];
	assert(type == TAG_DATA);

	if (m_network == nullptr)
		return;

	if ((m_netState == RPT_NET_STATE::AUDIO) || (m_netState == RPT_NET_STATE::DATA)) {
		unsigned char n = data[1U];

		if (n == 0U) {
			CSync::addDStarSync(data + 2U);
			m_netSlowData.start();
		} else {
			m_netSlowData.add(data + 2U);
		}

		if (m_netDataLookBackIndex == 0U) {
			m_netDataLookBack = new unsigned char[length];
			::memcpy(m_netDataLookBack, data, length);
			m_netDataLookBackLen = length;
			m_netDataLookBackIndex ++;
			return;
		} else if (m_netDataLookBackIndex == 1U) {
			m_netDataLookBackIndex ++;

			if (m_netState == RPT_NET_STATE::AUDIO){
				unsigned char type = m_netSlowData.getType(false);
				// as per specs chapter 6.1.3, this could be any value bewtween 0x81 and 0x9C as one fast data block can only contain 28 bytes 0x80 + 28d = 0x9C 
				if ((type & DSTAR_SLOW_DATA_TYPE_FASTDATA_MASK) == DSTAR_SLOW_DATA_TYPE_FASTDATA){
					LogMessage("D-Star, starting fast data mode (Net)");
					m_netState = RPT_NET_STATE::DATA;
				}
			}
			LogDebug("D-Star, processing lookback data (Net)");
			writeNetworkData(m_netDataLookBack, m_netDataLookBackLen);
			writeNetworkData(data, length);
			delete[] m_netDataLookBack;
			m_netDataLookBack = nullptr;
			m_netDataLookBackLen = 0U;
			return;
		}
	}

	if (m_netState == RPT_NET_STATE::AUDIO) {
	    unsigned char type = m_netSlowData.getType(false);
		// LogMessage("D-Star, slow data type: 0x%02X 0x%02X %i", type, type & DSTAR_SLOW_DATA_TYPE_FASTDATA_MASK, m_netN);
		// as per specs chapter 6.1.3, this could be any value bewtween 0x81 and 0x9C as one fast data block can only contain 28 bytes 0x80 + 28d = 0x9C 
		if ((type & DSTAR_SLOW_DATA_TYPE_FASTDATA_MASK) == DSTAR_SLOW_DATA_TYPE_FASTDATA){
			LogMessage("D-Star, starting fast data mode (Net)");
			m_netState = RPT_NET_STATE::DATA;
		}
	}

	if (m_netState == RPT_NET_STATE::DATA) {
		unsigned char type = m_netSlowData.getType(true);
		// as per specs chapter 6.1.3, this could be any value bewtween 0x81 and 0x9C as one fast data block can only contain 28 bytes 0x80 + 28d = 0x9C 
		if (((type & DSTAR_SLOW_DATA_TYPE_FASTDATA_MASK) == DSTAR_SLOW_DATA_TYPE_FASTDATA) && type > 0x9CU) {
			LogMessage("Invalid fast data type 0x%02X received, returning to audio mode (Net)", type);
			m_netState = RPT_NET_STATE::AUDIO;
		}
	}

	if (m_netState == RPT_NET_STATE::AUDIO) {
		unsigned char n = data[1U];

		if (::memcmp(data + 2U, DSTAR_NULL_AMBE_DATA_BYTES_SCRAMBLED, DSTAR_VOICE_FRAME_LENGTH_BYTES) != 0) {
			unsigned int errors = m_fec.regenerateDStar(data + 2U);
			LogDebug("D-Star, Net audio sequence no. %u, errs: %u/48 (%.1f%%)", n, errors, float(errors) / 0.48F);
			blankDTMF(data + 2U);
		}

		data[1U] = TAG_DATA;

		// Insert silence and reject if in the past
		bool ret = insertSilence(data + 1U, n);
		if (!ret)
			return;

		m_netBits += 48U;

		m_netN = n;

		if (m_netN != 0U) {
			const unsigned char* text = m_netSlowData.getText();
			if (text != nullptr)
				LogMessage("D-Star, network slow data text = \"%s\"", text);
		}
			
		m_packetTimer.start();
		m_netFrames++;

		writeQueueDataNet(data + 1U);
	}

	if (m_netState == RPT_NET_STATE::DATA) {
		m_netN = data[1U];
		LogDebug("D-Star, Net fast data sequence no. %u", m_netN);

		data[1U] = TAG_DATA;

		m_netBits += 48U;

		if (m_netN != 0U) {
			bool complete = m_netSlowData.isComplete();
			if (complete) {
				unsigned char type = m_netSlowData.getType();
				if ((type & DSTAR_SLOW_DATA_TYPE_FASTDATA_MASK) != DSTAR_SLOW_DATA_TYPE_FASTDATA) {
					LogMessage("D-Star, leaving fast data mode (Net)");
					m_netState = RPT_NET_STATE::AUDIO;
				}
			}
		}

		m_packetTimer.start();
		m_netFrames++;

		writeQueueDataNet(data + 1U);
	}
}

void CDStarControl::clock()
{
	unsigned int ms = m_interval.elapsed();
	m_interval.start();

	if (m_network != nullptr)
		writeNetwork();

	if (!m_enabled)
		return;

	m_ackTimer.clock(ms);
	if (m_ackTimer.isRunning() && m_ackTimer.hasExpired()) {
		sendAck();
		m_ackTimer.stop();
	}

	m_errTimer.clock(ms);
	if (m_errTimer.isRunning() && m_errTimer.hasExpired()) {
		sendError();
		m_errTimer.stop();
	}

	m_rfTimeoutTimer.clock(ms);
	m_netTimeoutTimer.clock(ms);

	if ((m_netState == RPT_NET_STATE::AUDIO) || (m_netState == RPT_NET_STATE::DATA)) {
		m_networkWatchdog.clock(ms);

		if (m_networkWatchdog.hasExpired()) {
			// We're received the header haven't we?
			unsigned char my1[DSTAR_LONG_CALLSIGN_LENGTH];
			unsigned char my2[DSTAR_SHORT_CALLSIGN_LENGTH];
			unsigned char your[DSTAR_LONG_CALLSIGN_LENGTH];
			m_netHeader.getMyCall1(my1);
			m_netHeader.getMyCall2(my2);
			m_netHeader.getYourCall(your);
			m_netFrames += 1U;

			LogMessage("D-Star, network watchdog has expired, %.1f seconds,  %u%% packet loss", float(m_netFrames) / 50.0F, (m_netLost * 100U) / m_netFrames);
			writeJSONNet("lost", float(m_netFrames) / 50.0F, float(m_netLost * 100U) / float(m_netFrames));
			writeEndNet();
		}
	}

	// Only insert silence on audio data
	if (m_netState == RPT_NET_STATE::AUDIO) {
		m_packetTimer.clock(ms);

		if (m_packetTimer.isRunning() && m_packetTimer.hasExpired()) {
			unsigned int elapsed = m_elapsed.elapsed();
			unsigned int frames = elapsed / DSTAR_FRAME_TIME;

			if (frames > m_netFrames) {
				unsigned int count = frames - m_netFrames;
				if (count > 15U) {
					LogDebug("D-Star, lost audio for 300ms filling in, elapsed: %ums, expected: %u, received: %u", elapsed, frames, m_netFrames);
					insertSilence(count - 2U);
				}
			}

			m_packetTimer.start();
		}
	}
}

void CDStarControl::writeQueueHeaderRF(const unsigned char *data)
{
	assert(data != nullptr);

	if (m_netState != RPT_NET_STATE::IDLE)
		return;

	if (m_rfTimeoutTimer.isRunning() && m_rfTimeoutTimer.hasExpired())
		return;

	unsigned char len = DSTAR_HEADER_LENGTH_BYTES + 1U;

	unsigned int space = m_queue.freeSpace();
	if (space < (len + 1U)) {
		LogError("D-Star, overflow in the D-Star RF queue");
		return;
	}

	m_queue.addData(&len, 1U);

	m_queue.addData(data, len);
}

void CDStarControl::writeQueueDataRF(const unsigned char *data)
{
	assert(data != nullptr);

	if (m_netState != RPT_NET_STATE::IDLE)
		return;

	if (m_rfTimeoutTimer.isRunning() && m_rfTimeoutTimer.hasExpired())
		return;

	unsigned char len = DSTAR_FRAME_LENGTH_BYTES + 1U;

	unsigned int space = m_queue.freeSpace();
	if (space < (len + 1U)) {
		LogError("D-Star, overflow in the D-Star RF queue");
		return;
	}

	m_queue.addData(&len, 1U);

	m_queue.addData(data, len);
}

void CDStarControl::writeQueueEOTRF()
{
	if (m_netState != RPT_NET_STATE::IDLE)
		return;

	if (m_rfTimeoutTimer.isRunning() && m_rfTimeoutTimer.hasExpired())
		return;

	unsigned char len = 1U;

	unsigned int space = m_queue.freeSpace();
	if (space < (len + 1U)) {
		LogError("D-Star, overflow in the D-Star RF queue");
		return;
	}

	m_queue.addData(&len, 1U);

	unsigned char data = TAG_EOT;
	m_queue.addData(&data, len);
}

void CDStarControl::writeQueueHeaderNet(const unsigned char *data)
{
	assert(data != nullptr);

	if (m_netTimeoutTimer.isRunning() && m_netTimeoutTimer.hasExpired())
		return;

	unsigned char len = DSTAR_HEADER_LENGTH_BYTES + 1U;

	unsigned int space = m_queue.freeSpace();
	if (space < (len + 1U)) {
		LogError("D-Star, overflow in the D-Star RF queue");
		return;
	}

	m_queue.addData(&len, 1U);

	m_queue.addData(data, len);
}

void CDStarControl::writeQueueDataNet(const unsigned char *data)
{
	assert(data != nullptr);

	if (m_netTimeoutTimer.isRunning() && m_netTimeoutTimer.hasExpired())
		return;

	unsigned char len = DSTAR_FRAME_LENGTH_BYTES + 1U;

	unsigned int space = m_queue.freeSpace();
	if (space < (len + 1U)) {
		LogError("D-Star, overflow in the D-Star RF queue");
		return;
	}

	m_queue.addData(&len, 1U);

	m_queue.addData(data, len);
}

void CDStarControl::writeQueueEOTNet()
{
	if (m_netTimeoutTimer.isRunning() && m_netTimeoutTimer.hasExpired())
		return;

	unsigned char len = 1U;

	unsigned int space = m_queue.freeSpace();
	if (space < (len + 1U)) {
		LogError("D-Star, overflow in the D-Star RF queue");
		return;
	}

	m_queue.addData(&len, 1U);

	unsigned char data = TAG_EOT;
	m_queue.addData(&data, len);
}

void CDStarControl::writeNetworkHeaderRF(const unsigned char* data)
{
	assert(data != nullptr);

	if (m_network == nullptr)
		return;

	// Don't send to the network if the timeout has expired
	if (m_rfTimeoutTimer.isRunning() && m_rfTimeoutTimer.hasExpired())
		return;

	m_network->writeHeader(data + 1U, DSTAR_HEADER_LENGTH_BYTES, m_netState != RPT_NET_STATE::IDLE);
}

void CDStarControl::writeNetworkDataRF(const unsigned char* data, unsigned int errors, bool end)
{
	assert(data != nullptr);

	if (m_network == nullptr)
		return;

	// Don't send to the network if the timeout has expired
	if (m_rfTimeoutTimer.isRunning() && m_rfTimeoutTimer.hasExpired())
		return;

	m_network->writeData(data + 1U, DSTAR_FRAME_LENGTH_BYTES, errors, end, m_netState != RPT_NET_STATE::IDLE);
}

bool CDStarControl::insertSilence(const unsigned char* data, unsigned char seqNo)
{
	assert(data != nullptr);

	// Check to see if we have any spaces to fill?
	unsigned int oldSeqNo = (m_netN + 1U) % 21U;
	if (oldSeqNo == seqNo) {
		// Just copy the data, nothing else to do here
		::memcpy(m_lastFrame, data, DSTAR_FRAME_LENGTH_BYTES + 1U);
		m_lastFrameValid = true;
		return true;
	}

	unsigned int count;
	if (seqNo > oldSeqNo)
		count = seqNo - oldSeqNo;
	else
		count = (21U + seqNo) - oldSeqNo;

	if (count >= 10U)
		return false;

	insertSilence(count);

	::memcpy(m_lastFrame, data, DSTAR_FRAME_LENGTH_BYTES + 1U);
	m_lastFrameValid = true;

	return true;
}

void CDStarControl::insertSilence(unsigned int count)
{
	unsigned char n = (m_netN + 1U) % 21U;

	for (unsigned int i = 0U; i < count; i++) {
		if (i < 3U && m_lastFrameValid) {
			if (n == 0U) {
				::memcpy(m_lastFrame + DSTAR_VOICE_FRAME_LENGTH_BYTES + 1U, DSTAR_NULL_SLOW_SYNC_BYTES, DSTAR_DATA_FRAME_LENGTH_BYTES);
				writeQueueDataNet(m_lastFrame);
			} else {
				::memcpy(m_lastFrame + DSTAR_VOICE_FRAME_LENGTH_BYTES + 1U, DSTAR_NULL_SLOW_DATA_BYTES, DSTAR_DATA_FRAME_LENGTH_BYTES);
				writeQueueDataNet(m_lastFrame);
			}
		} else {
			m_lastFrameValid = false;

			if (n == 0U)
				writeQueueDataNet(DSTAR_NULL_FRAME_SYNC_BYTES);
			else
				writeQueueDataNet(DSTAR_NULL_FRAME_DATA_BYTES);
		}

		m_netN = n;

		m_netFrames++;
		m_netLost++;

		n = (n + 1U) % 21U;
	}
}

void CDStarControl::blankDTMF(unsigned char* data) const
{
	assert(data != nullptr);

	// DTMF begins with these byte values
	if ((data[0] & DSTAR_DTMF_MASK[0]) == DSTAR_DTMF_SIG[0] && (data[1] & DSTAR_DTMF_MASK[1]) == DSTAR_DTMF_SIG[1] &&
		(data[2] & DSTAR_DTMF_MASK[2]) == DSTAR_DTMF_SIG[2] && (data[3] & DSTAR_DTMF_MASK[3]) == DSTAR_DTMF_SIG[3] &&
		(data[4] & DSTAR_DTMF_MASK[4]) == DSTAR_DTMF_SIG[4] && (data[5] & DSTAR_DTMF_MASK[5]) == DSTAR_DTMF_SIG[5] &&
		(data[6] & DSTAR_DTMF_MASK[6]) == DSTAR_DTMF_SIG[6] && (data[7] & DSTAR_DTMF_MASK[7]) == DSTAR_DTMF_SIG[7] &&
		(data[8] & DSTAR_DTMF_MASK[8]) == DSTAR_DTMF_SIG[8])
		::memcpy(data, DSTAR_NULL_AMBE_DATA_BYTES, DSTAR_VOICE_FRAME_LENGTH_BYTES);
}

void CDStarControl::sendAck()
{
	m_rfTimeoutTimer.stop();

	if (!m_ackReply)
		return;

	unsigned char user[DSTAR_LONG_CALLSIGN_LENGTH];
	m_rfHeader.getMyCall1(user);

	CDStarHeader header;
	header.setUnavailable(true);
	header.setMyCall1(m_callsign);
	header.setYourCall(user);
	header.setRPTCall1(m_gateway);
	header.setRPTCall2(m_callsign);

	unsigned char data[DSTAR_HEADER_LENGTH_BYTES + 1U];
	header.get(data + 1U);
	data[0U] = TAG_HEADER;

	writeQueueHeaderRF(data);

	writeQueueDataRF(DSTAR_NULL_FRAME_SYNC_BYTES);

	LINK_STATUS status = LINK_STATUS::NONE;
	unsigned char reflector[DSTAR_LONG_CALLSIGN_LENGTH];
	if (m_network != nullptr)
		m_network->getStatus(status, reflector);

	char text[40U];
	if ((m_ackMessage == DSTAR_ACK::RSSI) && (m_rssi != 0U)) {
		if ((status == LINK_STATUS::LINKED_DEXTRA) || (status == LINK_STATUS::LINKED_DPLUS) || (status == LINK_STATUS::LINKED_DCS) || (status == LINK_STATUS::LINKED_CCS) || (status == LINK_STATUS::LINKED_LOOPBACK)) {
			CUtils::removeChar(reflector, ' ');//remove space from reflector so all nicely fits onto 20 chars in case rssi < 99dBm
			::sprintf(text, "%-8.8s %.1f%% %ddBm        ", reflector, float(m_rfErrs * 100U) / float(m_rfBits), m_aveRSSI / int(m_rssiCountTotal));
		} else {
			::sprintf(text, "BER:%.1f%% %ddBm           ", float(m_rfErrs * 100U) / float(m_rfBits), m_aveRSSI / int(m_rssiCountTotal));
		}
	} else if ((m_ackMessage == DSTAR_ACK::SMETER) && (m_rssi != 0U)) {
		const int RSSI_S1 = -141;
		const int RSSI_S9 = -93;

		int signal = 0;
		int plus   = 0;
		int rssi   = m_aveRSSI / int(m_rssiCountTotal);

		if (rssi < RSSI_S1) {
			plus   = RSSI_S1 - rssi;
		} else if (rssi < RSSI_S9 && rssi >= RSSI_S1) {
			signal = ((rssi - RSSI_S1) / 6) + 1;
			plus   = (rssi - RSSI_S1) % 6;
		} else {
			plus   = rssi - RSSI_S9;
		}

		char signalText[15U];
		if (plus != 0)
			::sprintf(signalText, "S%d+%02d", signal, plus);
		else
			::sprintf(signalText, "S%d", signal);

		if ((status == LINK_STATUS::LINKED_DEXTRA) || (status == LINK_STATUS::LINKED_DPLUS) || (status == LINK_STATUS::LINKED_DCS) || (status == LINK_STATUS::LINKED_CCS) || (status == LINK_STATUS::LINKED_LOOPBACK))
			::sprintf(text, "%-8.8s %.1f%% %s           ", reflector, float(m_rfErrs * 100U) / float(m_rfBits), signalText);
		else
			::sprintf(text, "BER:%.1f%% %s             ", float(m_rfErrs * 100U) / float(m_rfBits), signalText);
	} else {
		if ((status == LINK_STATUS::LINKED_DEXTRA) || (status == LINK_STATUS::LINKED_DPLUS) || (status == LINK_STATUS::LINKED_DCS) || (status == LINK_STATUS::LINKED_CCS) || (status == LINK_STATUS::LINKED_LOOPBACK))
			::sprintf(text, "%-8.8s  BER: %.1f%%         ", reflector, float(m_rfErrs * 100U) / float(m_rfBits));
		else
			::sprintf(text, "BER: %.1f%%                 ", float(m_rfErrs * 100U) / float(m_rfBits));
	}

	m_rfSlowData.setText(text);

	::memcpy(data, DSTAR_NULL_FRAME_DATA_BYTES, DSTAR_FRAME_LENGTH_BYTES + 1U);

	for (unsigned int i = 0U; i < 19U; i++) {
		m_rfSlowData.getSlowData(data + 1U + DSTAR_VOICE_FRAME_LENGTH_BYTES);
		writeQueueDataRF(data);
	}

	writeQueueEOTRF();
}

void CDStarControl::sendError()
{
	unsigned char user[DSTAR_LONG_CALLSIGN_LENGTH];
	m_rfHeader.getMyCall1(user);

	CDStarHeader header;
	header.setUnavailable(true);
	header.setMyCall1(m_callsign);
	header.setYourCall(user);
	header.setRPTCall1(m_callsign);
	header.setRPTCall2(m_callsign);

	unsigned char data[DSTAR_HEADER_LENGTH_BYTES + 1U];
	header.get(data + 1U);
	data[0U] = TAG_HEADER;

	writeQueueHeaderRF(data);

	writeQueueDataRF(DSTAR_NULL_FRAME_SYNC_BYTES);

	LINK_STATUS status = LINK_STATUS::NONE;
	unsigned char reflector[DSTAR_LONG_CALLSIGN_LENGTH];
	if (m_network != nullptr)
		m_network->getStatus(status, reflector);

	char text[40U];
	if ((m_ackMessage == DSTAR_ACK::RSSI) && (m_rssi != 0U)) {
		if ((status == LINK_STATUS::LINKED_DEXTRA) || (status == LINK_STATUS::LINKED_DPLUS) || (status == LINK_STATUS::LINKED_DCS) || (status == LINK_STATUS::LINKED_CCS) || (status == LINK_STATUS::LINKED_LOOPBACK)) {
			CUtils::removeChar(reflector, ' ');//remove space from reflector so all nicely fits onto 20 chars in case rssi < 99dBm
			::sprintf(text, "%-8.8s %.1f%% %ddBm        ", reflector, float(m_rfErrs * 100U) / float(m_rfBits), m_aveRSSI / int(m_rssiCountTotal));
		} else {
			::sprintf(text, "BER:%.1f%% %ddBm           ", float(m_rfErrs * 100U) / float(m_rfBits), m_aveRSSI / int(m_rssiCountTotal));
		}
	} else if ((m_ackMessage == DSTAR_ACK::SMETER) && (m_rssi != 0U)) {
		const int RSSI_S1 = -141;
		const int RSSI_S9 = -93;

		int signal = 0;
		int plus   = 0;
		int rssi   = m_aveRSSI / int(m_rssiCountTotal);

		if (rssi < RSSI_S1) {
			plus   = RSSI_S1 - rssi;
		} else if (rssi < RSSI_S9 && rssi >= RSSI_S1) {
			signal = ((rssi - RSSI_S1) / 6) + 1;
			plus   = (rssi - RSSI_S1) % 6;
		} else {
			plus   = rssi - RSSI_S9;
		}

		char signalText[15U];
		if (plus != 0U)
			::sprintf(signalText, "S%d+%02d", signal, plus);
		else
			::sprintf(signalText, "S%d", signal);

		if ((status == LINK_STATUS::LINKED_DEXTRA) || (status == LINK_STATUS::LINKED_DPLUS) || (status == LINK_STATUS::LINKED_DCS) || (status == LINK_STATUS::LINKED_CCS) || (status == LINK_STATUS::LINKED_LOOPBACK))
			::sprintf(text, "%-8.8s %.1f%% %s           ", reflector, float(m_rfErrs * 100U) / float(m_rfBits), signalText);
		else
			::sprintf(text, "BER:%.1f%% %s             ", float(m_rfErrs * 100U) / float(m_rfBits), signalText);
	} else {
		if ((status == LINK_STATUS::LINKED_DEXTRA) || (status == LINK_STATUS::LINKED_DPLUS) || (status == LINK_STATUS::LINKED_DCS) || (status == LINK_STATUS::LINKED_CCS) || (status == LINK_STATUS::LINKED_LOOPBACK))
			::sprintf(text, "%-8.8s  BER: %.1f%%         ", reflector, float(m_rfErrs * 100U) / float(m_rfBits));
		else
			::sprintf(text, "BER: %.1f%%                 ", float(m_rfErrs * 100U) / float(m_rfBits));
	}

	m_rfSlowData.setText(text);

	::memcpy(data, DSTAR_NULL_FRAME_DATA_BYTES, DSTAR_FRAME_LENGTH_BYTES + 1U);

	for (unsigned int i = 0U; i < 19U; i++) {
		m_rfSlowData.getSlowData(data + 1U + DSTAR_VOICE_FRAME_LENGTH_BYTES);
		writeQueueDataRF(data);
	}

	writeQueueEOTRF();
}

bool CDStarControl::isBusy() const
{
	return (m_rfState != RPT_RF_STATE::LISTENING) || (m_netState != RPT_NET_STATE::IDLE);
}

void CDStarControl::enable(bool enabled)
{
	if (!enabled && m_enabled) {
		m_queue.clear();

		// Reset the RF section
		switch (m_rfState) {
		case RPT_RF_STATE::LISTENING:
		case RPT_RF_STATE::REJECTED:
		case RPT_RF_STATE::INVALID:
			break;

		default:
			if (m_rfTimeoutTimer.isRunning())
				LogMessage("D-Star, RF user has timed out");
			break;
		}
		m_rfState = RPT_RF_STATE::LISTENING;

		m_rfTimeoutTimer.stop();

		// Reset the networking section
		switch(m_netState) {
		case RPT_NET_STATE::IDLE:
			break;

		default:
			if (m_netTimeoutTimer.isRunning())
				LogMessage("D-Star, network user has timed out");
			break;
		}
		m_netState = RPT_NET_STATE::IDLE;

		m_lastFrameValid = false;

		m_netTimeoutTimer.stop();
		m_networkWatchdog.stop();
		m_packetTimer.stop();
	}

	m_enabled = enabled;
}

void CDStarControl::writeJSONRSSI()
{
	if (m_rssi == 0)
		return;

	if (m_rssiCountTotal >= RSSI_COUNT) {
		nlohmann::json json;

		json["timestamp"] = CUtils::createTimestamp();
		json["mode"]      = "D-Star";

		json["value"]     = m_rssiAccum / int(m_rssiCountTotal);

		WriteJSON("RSSI", json);

		m_rssiAccum      = 0;
		m_rssiCountTotal = 0U;
	}
}

void CDStarControl::writeJSONBER()
{
	if (m_bitsCount >= BER_COUNT) {
		nlohmann::json json;

		json["timestamp"] = CUtils::createTimestamp();
		json["mode"]      = "D-Star";

		json["value"]     = float(m_bitErrsAccum * 100U) / float(m_bitsCount);

		WriteJSON("BER", json);

		m_bitErrsAccum = 0U;
		m_bitsCount    = 1U;
	}
}

void CDStarControl::writeJSONText(const unsigned char* text)
{
	assert(text != nullptr);

	nlohmann::json json;

	json["timestamp"] = CUtils::createTimestamp();
	json["mode"]      = "D-Star";

	json["value"]     = std::string((char*)text);

	WriteJSON("Text", json);
}

void CDStarControl::writeJSONRF(const char* action, const unsigned char* my1, const unsigned char* my2, const unsigned char* your)
{
	assert(action != nullptr);
	assert(my1 != nullptr);
	assert(my2 != nullptr);
	assert(your != nullptr);

	nlohmann::json json;

	json["timestamp"]    = CUtils::createTimestamp();

	json["src_callsign"] = convertBuffer(my1, DSTAR_LONG_CALLSIGN_LENGTH);
	json["src_ext"]      = convertBuffer(my2, DSTAR_SHORT_CALLSIGN_LENGTH);
	json["dst_callsign"] = convertBuffer(your, DSTAR_LONG_CALLSIGN_LENGTH);

	json["source"]    = "rf";
	json["action"]    = action;
	json["reflector"] = "";

	WriteJSON("D-Star", json);
}

void CDStarControl::writeJSONRF(const char* action, float duration, float ber)
{
	assert(action != nullptr);

	nlohmann::json json;

	writeJSONRF(json, action, duration, ber);

	WriteJSON("D-Star", json);
}

void CDStarControl::writeJSONRF(const char* action, float duration, float ber, int minRSSI, int maxRSSI, int aveRSSI)
{
	assert(action != nullptr);

	nlohmann::json json;

	writeJSONRF(json, action, duration, ber);

	nlohmann::json rssi;
	rssi["min"] = minRSSI;
	rssi["max"] = maxRSSI;
	rssi["ave"] = aveRSSI;

	json["rssi"] = rssi;

	WriteJSON("D-Star", json);
}

void CDStarControl::writeJSONNet(const char* action, const unsigned char* my1, const unsigned char* my2, const unsigned char* your, const unsigned char* reflector)
{
	assert(action != nullptr);
	assert(my1 != nullptr);
	assert(my2 != nullptr);
	assert(your != nullptr);

	nlohmann::json json;

	json["timestamp"]    = CUtils::createTimestamp();

	json["src_callsign"] = convertBuffer(my1, DSTAR_LONG_CALLSIGN_LENGTH);
	json["src_ext"]      = convertBuffer(my2, DSTAR_SHORT_CALLSIGN_LENGTH);
	json["dst_callsign"] = convertBuffer(your, DSTAR_LONG_CALLSIGN_LENGTH);

	json["source"] = "network";
	json["action"] = action;

	if (reflector != nullptr)
		json["reflector"] = convertBuffer(reflector, DSTAR_LONG_CALLSIGN_LENGTH);
	else
		json["reflector"] = "";

	WriteJSON("D-Star", json);
}

void CDStarControl::writeJSONNet(const char* action, float duration, float loss)
{
	assert(action != nullptr);

	nlohmann::json json;

	json["timestamp"] = CUtils::createTimestamp();

	json["duration"] = duration;
	json["loss"]     = loss;

	json["action"] = action;

	WriteJSON("D-Star", json);
}

void CDStarControl::writeJSONRF(nlohmann::json& json, const char* action, float duration, float ber)
{
	assert(action != nullptr);

	json["timestamp"] = CUtils::createTimestamp();

	json["duration"] = duration;
	json["ber"]      = ber;

	json["action"] = action;
}

std::string CDStarControl::convertBuffer(const unsigned char* buffer, unsigned int length) const
{
	assert(buffer != nullptr);

	std::string callsign((char*)buffer, length);

	// Remove only the trailing padding: a field can start with spaces
	// ("       E" for echo, "       U" for unlink) or have a space
	// inside ("XLX672 E" for a reflector and module).
	size_t pos = callsign.find_last_not_of(' ');
	if (pos == std::string::npos)
		return "";

	return callsign.substr(0U, pos + 1U);
}

#endif
