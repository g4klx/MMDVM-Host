/*
 *   Copyright (C) 2019,2020,2021,2023,2024,2025 by Jonathan Naylor G4KLX
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

#ifndef	RemoteControl_H
#define	RemoteControl_H

#include "Defines.h"

#include <vector>
#include <string>

#include "MQTTConnection.h"

class CMMDVMHost;

enum class REMOTE_COMMAND {
	NONE,
	MODE_IDLE,
	MODE_LOCKOUT,
#if defined(USE_DSTAR)
	MODE_DSTAR,
#endif
#if defined(USE_DMR)
	MODE_DMR,
#endif
#if defined(USE_YSF)
	MODE_YSF,
#endif
#if defined(USE_P25)
	MODE_P25,
#endif
#if defined(USE_NXDN)
	MODE_NXDN,
#endif
#if defined(USE_FM)
	MODE_FM,
#endif
#if defined(USE_DSTAR)
	ENABLE_DSTAR,
#endif
#if defined(USE_DMR)
	ENABLE_DMR,
#endif
#if defined(USE_YSF)
	ENABLE_YSF,
#endif
#if defined(USE_P25)
	ENABLE_P25,
#endif
#if defined(USE_NXDN)
	ENABLE_NXDN,
#endif
#if defined(USE_FM)
	ENABLE_FM,
#endif
#if defined(USE_DSTAR)
	DISABLE_DSTAR,
#endif
#if defined(USE_DMR)
	DISABLE_DMR,
#endif
#if defined(USE_YSF)
	DISABLE_YSF,
#endif
#if defined(USE_P25)
	DISABLE_P25,
#endif
#if defined(USE_NXDN)
	DISABLE_NXDN,
#endif
#if defined(USE_FM)
	DISABLE_FM,
#endif
#if defined(USE_POCSAG)
	PAGE,
	PAGE_BCD,
	PAGE_A1,
	PAGE_A2,
#endif
	CW,
	LISTEN_ON,
	LISTEN_OFF,
	RELOAD,
	CONNECTION_STATUS,
	CONFIG_HOSTS,
	MODEM_INFO
};

class CRemoteControl {
public:
	CRemoteControl(class CMMDVMHost *host, CMQTTConnection* mqtt);
	~CRemoteControl();

	REMOTE_COMMAND getCommand(const std::string& command);

	unsigned int getArgCount() const;

	std::string  getArgString(unsigned int n) const;
	unsigned int getArgUInt(unsigned int n) const;
	signed int   getArgInt(unsigned int n) const;

private:
	CMMDVMHost*              m_host;
	CMQTTConnection*         m_mqtt;
	REMOTE_COMMAND           m_command;
	std::vector<std::string> m_args;
};

#endif
