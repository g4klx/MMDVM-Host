/*
 *   Copyright (C) 2015-2020,2023 by Jonathan Naylor G4KLX
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(NXDNControl_H)
#define	NXDNControl_H

#include "RSSIInterpolator.h"
#include "NXDNNetwork.h"
#include "NXDNDefines.h"
#include "NXDNLayer3.h"
#include "NXDNLookup.h"
#include "RingBuffer.h"
#include "StopWatch.h"
#include "NXDNLICH.h"
#include "Defines.h"
#include "Timer.h"
#include "Modem.h"

#if defined(USE_NXDN)

#include <string>

#include <nlohmann/json.hpp>

class CNXDNControl {
public:
	CNXDNControl(unsigned int ran, unsigned int id, bool selfOnly, INXDNNetwork* network, unsigned int timeout, bool duplex, bool remoteGateway, unsigned int jitter, CNXDNLookup* lookup, CRSSIInterpolator* rssiMapper);
	~CNXDNControl();

	bool writeModem(unsigned char* data, unsigned int len);

	unsigned int readModem(unsigned char* data);

	void clock(unsigned int ms);

	bool isBusy() const;

	void enable(bool enabled);

private:
	unsigned int               m_ran;
	unsigned int               m_id;
	bool                       m_selfOnly;
	INXDNNetwork*              m_network;
	bool                       m_duplex;
	bool                       m_remoteGateway;
	CNXDNLookup*               m_lookup;
	CRingBuffer<unsigned char> m_queue;
	RPT_RF_STATE               m_rfState;
	RPT_NET_STATE              m_netState;
	CTimer                     m_rfTimeoutTimer;
	CTimer                     m_netTimeoutTimer;
	CTimer                     m_packetTimer;
	CTimer                     m_networkWatchdog;
	CStopWatch                 m_elapsed;
	unsigned int               m_rfFrames;
	unsigned int               m_netFrames;
	unsigned int               m_rfErrs;
	unsigned int               m_rfBits;
	CNXDNLICH                  m_rfLastLICH;
	CNXDNLayer3                m_rfLayer3;
	CNXDNLayer3                m_netLayer3;
	unsigned char              m_rfMask;
	unsigned char              m_netMask;
	CRSSIInterpolator*         m_rssiMapper;
	int                        m_rssi;
	int                        m_maxRSSI;
	int                        m_minRSSI;
	int                        m_aveRSSI;
	unsigned int               m_rssiCountTotal;
	int                        m_rssiAccum;
	unsigned int               m_rssiCount;
	unsigned int               m_bitsCount;
	unsigned int               m_bitErrsAccum;
	bool                       m_enabled;
	// Network jitter buffer (gap concealment for the net->modem voice path)
	CRingBuffer<unsigned char> m_netJitter;
	unsigned int               m_playoutAccum;
	unsigned int               m_jitterSlots;
	unsigned int               m_netJitterCount;
	bool                       m_netBuffering;
	bool                       m_netEndPending;
	bool                       m_lastFrameValid;
	unsigned char              m_lastFrame[NXDN_FRAME_LENGTH_BYTES + 2U];

	bool processVoice(unsigned char usc, unsigned char option, unsigned char *data);
	bool processData(unsigned char option, unsigned char *data);

	void writeQueueRF(const unsigned char* data);
	void writeQueueNet(const unsigned char* data);
	void jitterEnqueue(const unsigned char* data);
	void playoutNet();
	void writeNetwork(const unsigned char* data, NXDN_NETWORK_MESSAGE_TYPE type);
	void writeNetwork();

	void scrambler(unsigned char* data) const;

	void writeEndRF(bool writeEnd = false);
	void writeEndNet();

	void writeJSONRSSI();
	void writeJSONBER(unsigned int bits, unsigned int errs);

	void writeJSONRF(const char* action, unsigned short srcId, const std::string& srcInfo, bool grp, unsigned short dstId);
	void writeJSONRF(const char* action, float duration, float ber);
	void writeJSONRF(const char* action, float duration, float ber, int minRSSI, int maxRSSI, int aveRSSI);

	void writeJSONNet(const char* action, unsigned short srcId, const std::string& srcInfo, bool grp, unsigned short dstId);
	void writeJSONNet(const char* action, unsigned short srcId, const std::string& srcInfo, bool grp, unsigned short dstId, unsigned char frames);
	void writeJSONNet(const char* action);
	void writeJSONNet(const char* action, float duration);

	void writeJSON(nlohmann::json& json, const char* action);
	void writeJSON(nlohmann::json& json, const char* source, const char* action, unsigned short srcId, const std::string& srcInfo, bool grp, unsigned short dstId);
};

#endif

#endif

