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

#include "FMNetwork.h"
#include "ListenOnly.h"
#include "Utils.h"
#include "Log.h"

#if defined(USE_FM)

#include <cstdio>
#include <cassert>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

const unsigned int BUFFER_LENGTH = 1500U;

CFMNetwork::CFMNetwork(const std::string& callsign, const std::string& localAddress, unsigned short localPort, const std::string& gatewayAddress, unsigned short gatewayPort, bool debug) :
m_callsign(callsign),
m_socket(localAddress, localPort),
m_addr(),
m_addrLen(0U),
m_debug(debug),
m_enabled(false),
m_buffer(2000U, "FM Network"),
m_seqNo(0U),
m_timer(1000U, 5U),
m_endOfTransmission(false)
{
	assert(!callsign.empty());
	assert(gatewayPort > 0U);
	assert(!gatewayAddress.empty());

	if (CUDPSocket::lookup(gatewayAddress, gatewayPort, m_addr, m_addrLen) != 0)
		m_addrLen = 0U;

	// Remove any trailing spaces/letters from the callsign
	size_t pos = callsign.find_first_of(' ');
	if (pos != std::string::npos)
		m_callsign = callsign.substr(0U, pos);
}

CFMNetwork::~CFMNetwork()
{
}

bool CFMNetwork::open()
{
	if (m_addrLen == 0U) {
		LogError("Unable to resolve the address of the FM Gateway");
		return false;
	}

	LogMessage("Opening FM network connection");

	bool ret = m_socket.open(m_addr);
	if (!ret)
		return false;

	m_timer.start();

	return true;
}

bool CFMNetwork::writeData(const float* data, unsigned int nSamples)
{
	// Listen-only: nothing heard on RF goes to the network.
	if (g_listenOnly)
		return true;

	assert(data != nullptr);
	assert(nSamples > 0U);

	if (m_seqNo == 0U) {
		bool ret = writeStart();
		if (!ret)
			return false;
	}

	assert(data != nullptr);
	assert(nSamples > 0U);

	uint8_t buffer[BUFFER_LENGTH];
	::memset(buffer, 0x00U, BUFFER_LENGTH);

	unsigned int length = 0U;

	buffer[length++] = 'F';
	buffer[length++] = 'M';
	buffer[length++] = 'D';

	for (unsigned int i = 0U; i < nSamples; i++) {
		short val = short(data[i] * 32767.0F + 0.5F);			// Changing audio format from float to S16LE

		buffer[length++] = (val >> 0) & 0xFFU;
		buffer[length++] = (val >> 8) & 0xFFU;
	}

	m_seqNo++;

	if (m_debug)
		CUtils::dump(1U, "FM Network Data Sent", buffer, length);

	return m_socket.write(buffer, length, m_addr, m_addrLen);
}

bool CFMNetwork::writeStart()
{
	// Listen-only: nothing heard on RF goes to the network.
	if (g_listenOnly)
		return true;

	uint8_t buffer[20U];

	unsigned int length = 0U;

	buffer[length++] = 'F';
	buffer[length++] = 'M';
	buffer[length++] = 'S';

	for (unsigned int i = 0U; i < m_callsign.size(); i++)
		buffer[length++] = m_callsign.at(i);

	buffer[length++] = '\0';

	if (m_debug)
		CUtils::dump(1U, "FM Network Data Sent", buffer, length);

	return m_socket.write(buffer, length, m_addr, m_addrLen);
}

bool CFMNetwork::writeEnd()
{
	// Listen-only: nothing heard on RF goes to the network.
	if (g_listenOnly)
		return true;

	uint8_t buffer[5U];

	buffer[0U] = 'F';
	buffer[1U] = 'M';
	buffer[2U] = 'E';

	m_seqNo = 0U;

	if (m_debug)
		CUtils::dump(1U, "FM Network Data Sent", buffer, 3U);

	return m_socket.write(buffer, 3U, m_addr, m_addrLen);
}

bool CFMNetwork::writePing()
{
	uint8_t buffer[5U];

	buffer[0U] = 'F';
	buffer[1U] = 'M';
	buffer[2U] = 'P';

	if (m_debug)
		CUtils::dump(1U, "FM Network Data Sent", buffer, 3U);

	return m_socket.write(buffer, 3U, m_addr, m_addrLen);
}

void CFMNetwork::clock(unsigned int ms)
{
	m_timer.clock(ms);
	if (m_timer.isRunning() && m_timer.hasExpired()) {
		writePing();
		m_timer.start();
	}

	unsigned char buffer[BUFFER_LENGTH];

	sockaddr_storage addr;
	unsigned int addrlen;
	int length = m_socket.read(buffer, BUFFER_LENGTH, addr, addrlen);
	if (length <= 0)
		return;

	// Check if the data is for us
	if (!CUDPSocket::match(addr, m_addr, IPMATCHTYPE::ADDRESS_AND_PORT)) {
		LogMessage("FM packet received from an invalid source");
		return;
	}

	if (!m_enabled)
		return;

	// Invalid packet type?
	if (::memcmp(buffer, "FM", 2U) != 0)
		return;

	if (::memcmp(buffer, "FMP", 3U) == 0)
		return;

	if (m_debug)
		CUtils::dump(1U, "FM Network Data Received", buffer, length);

	if (::memcmp(buffer, "FME", 3U) == 0) {
		m_endOfTransmission = true;
		return;
	}

	if (::memcmp(buffer, "FMD", 3U) != 0)
		return;

	m_buffer.addData(buffer + 3U, length - 3U);
}

unsigned int CFMNetwork::readData(float* out, unsigned int nOut)
{
	assert(out != nullptr);
	assert(nOut > 0U);

	unsigned int bytes = m_buffer.dataSize() / sizeof(unsigned short);
	if (bytes == 0U)
		return 0U;

	if (bytes < nOut)
		nOut = bytes;

	unsigned char buffer[BUFFER_LENGTH];
	m_buffer.getData(buffer, nOut * sizeof(unsigned short));

	for (unsigned int i = 0U; i < nOut; i++) {
		short val = ((buffer[i * 2U + 0U] & 0xFFU) << 0) + ((buffer[i * 2U + 1U] & 0xFFU) << 8);
		out[i] = float(val) / 32767.0F;
	}

	return nOut;
}

bool CFMNetwork::readEnd()
{
	if (m_endOfTransmission) {
		m_endOfTransmission = false;
		return true;
	}

	return false;
}

void CFMNetwork::reset()
{
	m_buffer.clear();
	m_endOfTransmission = false;
}

void CFMNetwork::close()
{
	m_socket.close();

	LogMessage("Closing FM network connection");
}

void CFMNetwork::enable(bool enabled)
{
	if (enabled && !m_enabled)
		reset();
	else if (!enabled && m_enabled)
		reset();

	m_enabled = enabled;
}

#endif

