/*
 *   Copyright (C) 2019,2020,2021,2023,2024,2025,2026 by Jonathan Naylor G4KLX
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

#include "RemoteControl.h"
#include "MMDVM-Host.h"
#include "Log.h"

#include <cstdio>
#include <cstdlib>
#include <cassert>

const unsigned int SET_MODE_ARGS = 2U;
const unsigned int ENABLE_ARGS = 2U;
const unsigned int DISABLE_ARGS = 2U;
const unsigned int PAGE_ARGS = 3U;
const unsigned int CW_ARGS = 2U;


CRemoteControl::CRemoteControl(CMMDVMHost *host, CMQTTConnection* mqtt) :
m_host(host),
m_mqtt(mqtt),
m_command(REMOTE_COMMAND::NONE),
m_args()
{
	assert(host != nullptr);
	assert(mqtt != nullptr);
}

CRemoteControl::~CRemoteControl()
{
}

REMOTE_COMMAND CRemoteControl::getCommand(const std::string& command)
{
	m_command = REMOTE_COMMAND::NONE;
	m_args.clear();

	std::string reply = "OK";

	// Parse the original command into a vector of strings.
	size_t start = command.find_first_not_of(' ');
	while (start != std::string::npos) {
		size_t end = command.find_first_of(' ', start);

		m_args.push_back(command.substr(start, end - start));

		start = command.find_first_not_of(' ', end);
	}

	if (m_args.at(0U) == "mode" && m_args.size() >= SET_MODE_ARGS) {
		// Mode command is in the form of "mode <mode> [<timeout>|fixed]"
		if (m_args.at(1U) == "idle")
			m_command = REMOTE_COMMAND::MODE_IDLE;
		else if (m_args.at(1U) == "lockout")
			m_command = REMOTE_COMMAND::MODE_LOCKOUT;
#if defined(USE_DSTAR)
		else if (m_args.at(1U) == "d-star")
			m_command = REMOTE_COMMAND::MODE_DSTAR;
#endif
#if defined(USE_DMR)
		else if (m_args.at(1U) == "dmr")
			m_command = REMOTE_COMMAND::MODE_DMR;
#endif
#if defined(USE_YSF)
		else if (m_args.at(1U) == "ysf")
			m_command = REMOTE_COMMAND::MODE_YSF;
#endif
#if defined(USE_P25)
		else if (m_args.at(1U) == "p25")
			m_command = REMOTE_COMMAND::MODE_P25;
#endif
#if defined(USE_NXDN)
		else if (m_args.at(1U) == "nxdn")
			m_command = REMOTE_COMMAND::MODE_NXDN;
#endif
		else
			reply = "KO";
	} else if (m_args.at(0U) == "enable" && m_args.size() >= ENABLE_ARGS) {
#if defined(USE_DSTAR)
		if (m_args.at(1U) == "dstar")
			m_command = REMOTE_COMMAND::ENABLE_DSTAR;
		else
#endif
#if defined(USE_DMR)
		if (m_args.at(1U) == "dmr")
			m_command = REMOTE_COMMAND::ENABLE_DMR;
		else
#endif
#if defined(USE_YSF)
		if (m_args.at(1U) == "ysf")
			m_command = REMOTE_COMMAND::ENABLE_YSF;
		else
#endif
#if defined(USE_P25)
		if (m_args.at(1U) == "p25")
			m_command = REMOTE_COMMAND::ENABLE_P25;
		else
#endif
#if defined(USE_NXDN)
		if (m_args.at(1U) == "nxdn")
			m_command = REMOTE_COMMAND::ENABLE_NXDN;
		else
#endif
#if defined(USE_FM)
		if (m_args.at(1U) == "fm")
			m_command = REMOTE_COMMAND::ENABLE_FM;
		else
#endif
			reply = "KO";
	} else if (m_args.at(0U) == "disable" && m_args.size() >= DISABLE_ARGS) {
#if defined(USE_DSTAR)
		if (m_args.at(1U) == "dstar")
			m_command = REMOTE_COMMAND::DISABLE_DSTAR;
		else
#endif
#if defined(USE_DMR)
		if (m_args.at(1U) == "dmr")
			m_command = REMOTE_COMMAND::DISABLE_DMR;
		else
#endif
#if defined(USE_YSF)
		if (m_args.at(1U) == "ysf")
			m_command = REMOTE_COMMAND::DISABLE_YSF;
		else
#endif
#if defined(USE_P25)
		if (m_args.at(1U) == "p25")
			m_command = REMOTE_COMMAND::DISABLE_P25;
		else
#endif
#if defined(USE_NXDN)
		if (m_args.at(1U) == "nxdn")
			m_command = REMOTE_COMMAND::DISABLE_NXDN;
		else
#endif
#if defined(USE_FM)
		if (m_args.at(1U) == "fm")
			m_command = REMOTE_COMMAND::DISABLE_FM;
		else
#endif
			reply = "KO";
#if defined(USE_POCSAG)
	} else if (m_args.at(0U) == "page" && m_args.size() >= PAGE_ARGS) {
		// Page command is in the form of "page <ric> <message>"
		m_command = REMOTE_COMMAND::PAGE;
	} else if (m_args.at(0U) == "page_bcd" && m_args.size() >= PAGE_ARGS) {
		// BCD page command is in the form of "page_bcd <ric> <bcd message>"
		m_command = REMOTE_COMMAND::PAGE_BCD;
	} else if (m_args.at(0U) == "page_a1" && m_args.size() == 2) {
		// Alert1 page command is in the form of "page_a1 <ric>"
		m_command = REMOTE_COMMAND::PAGE_A1;
	} else if (m_args.at(0U) == "page_a2" && m_args.size() >= PAGE_ARGS) {
		// Alert2 page command is in the form of "page_a2 <ric> <message>"
		m_command = REMOTE_COMMAND::PAGE_A2;
#endif
	} else if (m_args.at(0U) == "cw" && m_args.size() >= CW_ARGS) {
		// CW command is in the form of "cw <message>"
		m_command = REMOTE_COMMAND::CW;
	} else if (m_args.at(0U) == "listen" && m_args.size() >= 2U) {
		// "listen on" (hear RF, never transmit, networks off) or "listen off"
		if (m_args.at(1U) == "on")
			m_command = REMOTE_COMMAND::LISTEN_ON;
		else if (m_args.at(1U) == "off")
			m_command = REMOTE_COMMAND::LISTEN_OFF;
		else
			reply = "KO";
	} else if (m_args.at(0U) == "reload") {
		// Reload command is in the form of "reload"
		m_command = REMOTE_COMMAND::RELOAD;
	} else if (m_args.at(0U) == "status") {
		if (m_host != nullptr) {
			m_host->buildNetworkStatusString(reply);
		} else {
			reply = "KO";
		}

		m_command = REMOTE_COMMAND::CONNECTION_STATUS;
	} else if (m_args.at(0U) == "hosts") {
		if (m_host != nullptr) {
			m_host->buildNetworkHostsString(reply);
		} else {
			reply = "KO";
		}

		m_command = REMOTE_COMMAND::CONFIG_HOSTS;
	} else if (m_args.at(0U) == "modem") {
		// The modem's details as JSON, the same as the "Modem" message
		// sent when it was opened.
		if (m_host != nullptr) {
			m_host->buildModemInfoString(reply);
		} else {
			reply = "KO";
		}

		m_command = REMOTE_COMMAND::MODEM_INFO;
	} else {
		reply = "KO";
	}

	char buffer[200U];
	::snprintf(buffer, 200, "%s remote command of \"%s\" received", ((m_command == REMOTE_COMMAND::NONE) ? "Invalid" : "Valid"), command.c_str());

	if (m_command == REMOTE_COMMAND::NONE) {
		m_args.clear();
		LogWarning("%s", buffer);
	} else {
		LogMessage("%s", buffer);
	}
		
	m_mqtt->publish("response", reply.c_str());
	
	return m_command;
}

unsigned int CRemoteControl::getArgCount() const
{
	switch (m_command) {
		case REMOTE_COMMAND::MODE_IDLE:
		case REMOTE_COMMAND::MODE_LOCKOUT:
#if defined(USE_DSTAR)
		case REMOTE_COMMAND::MODE_DSTAR:
#endif
#if defined(USE_DMR)
		case REMOTE_COMMAND::MODE_DMR:
#endif
#if defined(USE_YSF)
		case REMOTE_COMMAND::MODE_YSF:
#endif
#if defined(USE_P25)
		case REMOTE_COMMAND::MODE_P25:
#endif
#if defined(USE_NXDN)
		case REMOTE_COMMAND::MODE_NXDN:
#endif
			return (unsigned int)m_args.size() - SET_MODE_ARGS;
#if defined(USE_POCSAG)
		case REMOTE_COMMAND::PAGE:
		case REMOTE_COMMAND::PAGE_BCD:
		case REMOTE_COMMAND::PAGE_A1:
		case REMOTE_COMMAND::PAGE_A2:
			return (unsigned int)m_args.size() - 1U;
#endif
		case REMOTE_COMMAND::CW:
			return (unsigned int)m_args.size() - 1U;
		default:
			return 0U;
	}
}

std::string CRemoteControl::getArgString(unsigned int n) const
{
	switch (m_command) {
		case REMOTE_COMMAND::MODE_IDLE:
		case REMOTE_COMMAND::MODE_LOCKOUT:
#if defined(USE_DSTAR)
		case REMOTE_COMMAND::MODE_DSTAR:
#endif
#if defined(USE_DMR)
		case REMOTE_COMMAND::MODE_DMR:
#endif
#if defined(USE_YSF)
		case REMOTE_COMMAND::MODE_YSF:
#endif
#if defined(USE_P25)
		case REMOTE_COMMAND::MODE_P25:
#endif
#if defined(USE_NXDN)
		case REMOTE_COMMAND::MODE_NXDN:
#endif
			n += SET_MODE_ARGS;
			break;
#if defined(USE_POCSAG)
		case REMOTE_COMMAND::PAGE:
		case REMOTE_COMMAND::PAGE_BCD:
		case REMOTE_COMMAND::PAGE_A1:
		case REMOTE_COMMAND::PAGE_A2:
			n += 1U;
			break;
#endif
		case REMOTE_COMMAND::CW:
			n += 1U;
			break;
		default:
			return "";
	}

	if (n >= m_args.size())
		return "";

	return m_args.at(n);
}

unsigned int CRemoteControl::getArgUInt(unsigned int n) const
{
	return (unsigned int)::atoi(getArgString(n).c_str());
}

int CRemoteControl::getArgInt(unsigned int n) const
{
	return ::atoi(getArgString(n).c_str());
}
