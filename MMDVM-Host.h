/*
 *   Copyright (C) 2015-2021,2023 by Jonathan Naylor G4KLX
 *   Copyright (C) 2015-2021,2025 by Jonathan Naylor G4KLX
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

#if !defined(MMDVMHOST_H)
#define	MMDVMHOST_H

#include "RemoteControl.h"
#include "POCSAGNetwork.h"
#include "POCSAGControl.h"
#include "DStarNetwork.h"
#include "NXDNNetwork.h"
#include "DStarControl.h"
#include "DMRControl.h"
#include "YSFControl.h"
#include "P25Control.h"
#include "NXDNControl.h"
#include "NXDNLookup.h"
#include "YSFNetwork.h"
#include "P25Network.h"
#include "DMRNetwork.h"
#include "FMNetwork.h"
#include "DMRLookup.h"
#include "FMControl.h"
#include "Defines.h"
#include "Timer.h"
#include "Modem.h"
#include "Conf.h"

#include <string>

class CMMDVMHost
{
public:
	CMMDVMHost(const std::string& confFile);
	~CMMDVMHost();

	int run();

	void buildNetworkStatusString(std::string &str);
	void buildNetworkHostsString(std::string &str);

private:
	void setListenOnly(bool on);
	CConf           m_conf;
	CModem*         m_modem;
#if defined(USE_DSTAR)
	CDStarControl*  m_dstar;
#endif
#if defined(USE_DMR)
	CDMRControl*    m_dmr;
#endif
#if defined(USE_YSF)
	CYSFControl*    m_ysf;
#endif
#if defined(USE_P25)
	CP25Control*    m_p25;
#endif
#if defined(USE_NXDN)
	CNXDNControl*   m_nxdn;
#endif
#if defined(USE_POCSAG)
	CPOCSAGControl* m_pocsag;
#endif
#if defined(USE_FM)
	CFMControl*     m_fm;
#endif
#if defined(USE_DSTAR)
	CDStarNetwork*  m_dstarNetwork;
#endif
#if defined(USE_DMR)
	CDMRNetwork*    m_dmrNetwork;
#endif
#if defined(USE_YSF)
	CYSFNetwork*    m_ysfNetwork;
#endif
#if defined(USE_P25)
	CP25Network*    m_p25Network;
#endif
#if defined(USE_NXDN)
	INXDNNetwork*   m_nxdnNetwork;
#endif
#if defined(USE_POCSAG)
	CPOCSAGNetwork* m_pocsagNetwork;
#endif
#if defined(USE_FM)
	CFMNetwork*     m_fmNetwork;
#endif
	unsigned char   m_mode;
#if defined(USE_DSTAR)
	unsigned int    m_dstarRFModeHang;
#endif
#if defined(USE_DMR)
	unsigned int    m_dmrRFModeHang;
#endif
#if defined(USE_YSF)
	unsigned int    m_ysfRFModeHang;
#endif
#if defined(USE_P25)
	unsigned int    m_p25RFModeHang;
#endif
#if defined(USE_NXDN)
	unsigned int    m_nxdnRFModeHang;
#endif
#if defined(USE_FM)
	unsigned int    m_fmRFModeHang;
#endif
#if defined(USE_DSTAR)
	unsigned int    m_dstarNetModeHang;
#endif
#if defined(USE_DMR)
	unsigned int    m_dmrNetModeHang;
#endif
#if defined(USE_YSF)
	unsigned int    m_ysfNetModeHang;
#endif
#if defined(USE_P25)
	unsigned int    m_p25NetModeHang;
#endif
#if defined(USE_NXDN)
	unsigned int    m_nxdnNetModeHang;
#endif
#if defined(USE_POCSAG)
	unsigned int    m_pocsagNetModeHang;
#endif
#if defined(USE_FM)
	unsigned int    m_fmNetModeHang;
#endif
	CTimer          m_modeTimer;
#if defined(USE_DMR)
	CTimer          m_dmrTXTimer;
#endif
	CTimer          m_cwIdTimer;
	bool            m_duplex;
	unsigned int    m_timeout;
	bool            m_dstarEnabled;
	bool            m_dmrEnabled;
	bool            m_ysfEnabled;
	bool            m_p25Enabled;
	bool            m_nxdnEnabled;
	bool            m_pocsagEnabled;
	bool            m_fmEnabled;
	bool            m_listenOnly;
	unsigned int    m_cwIdTime;
#if defined(USE_DMR) || defined(USE_P25)
	CDMRLookup*     m_dmrLookup;
#endif
#if defined(USE_NXDN)
	CNXDNLookup*    m_nxdnLookup;
#endif
	std::string     m_callsign;
	unsigned int    m_id;
	std::string     m_cwCallsign;
	bool            m_lockFileEnabled;
	std::string     m_lockFileName;
	CRemoteControl* m_remoteControl;
	bool            m_fixedMode;

	CTimer          m_serialTimer;
	unsigned char*  m_serialBuffer;
	unsigned int    m_serialStart;
	unsigned int    m_serialLength;

	void readParams();
	bool createModem();
#if defined(USE_DSTAR)
	bool createDStarNetwork();
#endif
#if defined(USE_DMR)
	bool createDMRNetwork();
#endif
#if defined(USE_YSF)
	bool createYSFNetwork();
#endif
#if defined(USE_P25)
	bool createP25Network();
#endif
#if defined(USE_NXDN)
	bool createNXDNNetwork();
#endif
#if defined(USE_POCSAG)
	bool createPOCSAGNetwork();
#endif
#if defined(USE_FM)
	bool createFMNetwork();
#endif
	void writeSerial(const unsigned char* message, unsigned int length);
	void remoteControl(const std::string& commandString);
	void processModeCommand(unsigned char mode, unsigned int timeout);
	void processEnableCommand(bool& mode, bool enabled);

	void setMode(unsigned char mode);

	void createLockFile(const char* mode) const;
	void removeLockFile() const;

	void writeJSONMode(const std::string& mode);
	void writeJSONMessage(const std::string& message);

	static void onDisplay(const unsigned char* message, unsigned int length);
	static void onQSOInfo(const unsigned char* message, unsigned int length);
	static void onCommand(const unsigned char* command, unsigned int length);
};

#endif
