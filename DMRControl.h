/*
 *   Copyright (C) 2015-2021,2023,2025 by Jonathan Naylor G4KLX
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

#if !defined(DMRControl_H)
#define	DMRControl_H

#include "RSSIInterpolator.h"
#include "DMRNetwork.h"
#include "DMRLookup.h"
#include "DMRSlot.h"
#include "DMRData.h"
#include "Modem.h"

#include "Defines.h"

#if defined(USE_DMR)

#include <vector>

class CDMRControl {
public:
	CDMRControl(unsigned int id, unsigned int colorCode, unsigned int callHang, bool selfOnly, bool embeddedLCOnly, bool dumpTAData, const std::vector<unsigned int>& prefixes, const std::vector<unsigned int>& blacklist, const std::vector<unsigned int>& whitelist, const std::vector<unsigned int>& slot1TGWhitelist, const std::vector<unsigned int>& slot2TGWhitelist, unsigned int timeout, CModem* modem, CDMRNetwork* network, bool duplex, CDMRLookup* lookup, CRSSIInterpolator* rssi, unsigned int jitter, DMR_OVCM ovcm, bool protect, unsigned int duplexControlTG);
	~CDMRControl();

	bool processWakeup(const unsigned char* data);

	bool writeModemSlot1(unsigned char* data, unsigned int len);
	bool writeModemSlot2(unsigned char* data, unsigned int len);

	unsigned int readModemSlot1(unsigned char* data);
	unsigned int readModemSlot2(unsigned char* data);

	void clock();

	bool isBusy() const;

	void enable(bool enabled);

private:
	unsigned int m_colorCode;
	CModem*      m_modem;
	CDMRNetwork* m_network;
	CDMRSlot     m_slot1;
	CDMRSlot     m_slot2;
	CDMRLookup*  m_lookup;
};

#endif

#endif

