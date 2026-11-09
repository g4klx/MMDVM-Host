/*
 *   Copyright (C) 2020,2021,2023,2024,2025 by Jonathan Naylor G4KLX
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

#if !defined(FMNetwork_H)
#define	FMNetwork_H

#include "RingBuffer.h"
#include "UDPSocket.h"
#include "Defines.h"
#include "Timer.h"

#if defined(USE_FM)

#include <cstdint>
#include <string>


class CFMNetwork {
public:
	CFMNetwork(const std::string& callsign, const std::string& localAddress, unsigned short localPort, const std::string& gatewayAddress, unsigned short gatewayPort, bool debug);
	~CFMNetwork();

	bool open();

	void enable(bool enabled);

	bool writeData(const float* data, unsigned int nSamples);

	bool writeEnd();

	unsigned int readData(float* out, unsigned int nOut);

	bool readEnd();

	void reset();

	void close();

	void clock(unsigned int ms);

private:
	std::string         m_callsign;
	CUDPSocket          m_socket;
	sockaddr_storage    m_addr;
	unsigned int        m_addrLen;
	bool                m_debug;
	bool                m_enabled;
	CRingBuffer<unsigned char> m_buffer;
	unsigned int        m_seqNo;
	CTimer              m_timer;
	bool                m_endOfTransmission;

	bool writeStart();
	bool writePing();
};

#endif

#endif

