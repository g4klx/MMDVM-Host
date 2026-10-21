/*
 *   Copyright (C) 2015-2021,2023,2024,2025,2026 by Jonathan Naylor G4KLX
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

#include "MMDVM-Host.h"
#include "NXDNKenwoodNetwork.h"
#include "NXDNIcomNetwork.h"
#include "RSSIInterpolator.h"
#include "NullController.h"
#include "UARTController.h"
#if defined(__linux__)
#include "I2CController.h"
#endif
#include "UDPController.h"
#include "MQTTConnection.h"
#include "DStarDefines.h"
#include "Version.h"
#include "StopWatch.h"
#include "Thread.h"
#include "Utils.h"
#include "Log.h"
#include "GitVersion.h"

#include <cstdio>
#include <vector>

#include <cstdlib>

#include <nlohmann/json.hpp>

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
const char* DEFAULT_INI_FILE = "MMDVM-Host.ini";
#else
const char* DEFAULT_INI_FILE = "/etc/MMDVM-Host.ini";
#endif

static bool m_killed = false;
static int  m_signal = 0;
static bool m_reload = false;

// In Log.cpp
extern CMQTTConnection* m_mqtt;

#if !defined(_WIN32) && !defined(_WIN64)
static void sigHandler(int signum)
{
	m_killed = true;
	m_signal = signum;
}
#endif

static CMMDVMHost* host = nullptr;

const char* HEADER1 = "This software is for use on amateur radio networks only,";
const char* HEADER2 = "it is to be used for educational purposes only. Its use on";
const char* HEADER3 = "commercial networks is strictly prohibited.";
const char* HEADER4 = "Copyright(C) 2015-2026 by Jonathan Naylor, G4KLX and others";

int main(int argc, char** argv)
{
	const char* iniFile = DEFAULT_INI_FILE;
	if (argc > 1) {
 		for (int currentArg = 1; currentArg < argc; ++currentArg) {
			std::string arg = argv[currentArg];
			if ((arg == "-v") || (arg == "--version")) {
				::fprintf(stdout, "MMDVM-Host version %s git #%.7s\n", VERSION, gitversion);
				return 0;
			} else if (arg.substr(0,1) == "-") {
				::fprintf(stderr, "Usage: MMDVM-Host [-v|--version] [filename]\n");
				return 1;
			} else {
				iniFile = argv[currentArg];
			}
		}
	}

#if !defined(_WIN32) && !defined(_WIN64)
	::signal(SIGINT,  sigHandler);
	::signal(SIGTERM, sigHandler);
	::signal(SIGHUP,  sigHandler);
#endif

	int ret = 0;

	do {
		m_signal = 0;
		m_killed = false;

		host = new CMMDVMHost(std::string(iniFile));
		ret = host->run();

		delete host;
		host = nullptr;

		switch (m_signal) {
			case 0:
				break;
			case 2:
				::LogInfo("MMDVM-Host-%s exited on receipt of SIGINT", VERSION);
				break;
			case 15:
				::LogInfo("MMDVM-Host-%s exited on receipt of SIGTERM", VERSION);
				break;
			case 1:
				::LogInfo("MMDVM-Host-%s is restarting on receipt of SIGHUP", VERSION);
				m_reload = true;
				break;
			default:
				::LogInfo("MMDVM-Host-%s exited on receipt of an unknown signal", VERSION);
				break;
		}
	} while (m_reload || (m_signal == 1));

	::LogFinalise();

	return ret;
}

CMMDVMHost::CMMDVMHost(const std::string& confFile) :
m_conf(confFile),
m_modem(nullptr),
#if defined(USE_DSTAR)
m_dstar(nullptr),
#endif
#if defined(USE_DMR)
m_dmr(nullptr),
#endif
#if defined(USE_YSF)
m_ysf(nullptr),
#endif
#if defined(USE_P25)
m_p25(nullptr),
#endif
#if defined(USE_NXDN)
m_nxdn(nullptr),
#endif
#if defined(USE_POCSAG)
m_pocsag(nullptr),
#endif
#if defined(USE_FM)
m_fm(nullptr),
#endif
#if defined(USE_DSTAR)
m_dstarNetwork(nullptr),
#endif
#if defined(USE_DMR)
m_dmrNetwork(nullptr),
#endif
#if defined(USE_YSF)
m_ysfNetwork(nullptr),
#endif
#if defined(USE_P25)
m_p25Network(nullptr),
#endif
#if defined(USE_NXDN)
m_nxdnNetwork(nullptr),
#endif
#if defined(USE_POCSAG)
m_pocsagNetwork(nullptr),
#endif
#if defined(USE_FM)
m_fmNetwork(nullptr),
#endif
m_mode(MODE_IDLE),
#if defined(USE_DSTAR)
m_dstarRFModeHang(10U),
#endif
#if defined(USE_DMR)
m_dmrRFModeHang(10U),
#endif
#if defined(USE_YSF)
m_ysfRFModeHang(10U),
#endif
#if defined(USE_P25)
m_p25RFModeHang(10U),
#endif
#if defined(USE_NXDN)
m_nxdnRFModeHang(10U),
#endif
#if defined(USE_FM)
m_fmRFModeHang(10U),
#endif
#if defined(USE_DSTAR)
m_dstarNetModeHang(3U),
#endif
#if defined(USE_DMR)
m_dmrNetModeHang(3U),
#endif
#if defined(USE_YSF)
m_ysfNetModeHang(3U),
#endif
#if defined(USE_P25)
m_p25NetModeHang(3U),
#endif
#if defined(USE_NXDN)
m_nxdnNetModeHang(3U),
#endif
#if defined(USE_POCSAG)
m_pocsagNetModeHang(3U),
#endif
#if defined(USE_FM)
m_fmNetModeHang(3U),
#endif
m_modeTimer(1000U),
#if defined(USE_DMR)
m_dmrTXTimer(1000U),
#endif
m_cwIdTimer(1000U),
m_duplex(false),
m_timeout(180U),
m_dstarEnabled(false),
m_dmrEnabled(false),
m_ysfEnabled(false),
m_p25Enabled(false),
m_nxdnEnabled(false),
m_pocsagEnabled(false),
m_fmEnabled(false),
m_listenOnly(false),
m_cwIdTime(0U),
#if defined(USE_DMR) || defined(USE_P25)
m_dmrLookup(nullptr),
#endif
#if defined(USE_NXDN)
m_nxdnLookup(nullptr),
#endif
m_callsign(),
m_id(0U),
m_cwCallsign(),
m_lockFileEnabled(false),
m_lockFileName(),
m_remoteControl(nullptr),
m_fixedMode(false),
m_serialTimer(1000U, 0U, 210U),	// 252 bytes at 9600 bps
m_serialBuffer(nullptr),
m_serialStart(0U),
m_serialLength(0U)
{
	CUDPSocket::startup();

	m_serialBuffer = new unsigned char[5000U];
}

CMMDVMHost::~CMMDVMHost()
{
	CUDPSocket::shutdown();

	delete[] m_serialBuffer;
}

int CMMDVMHost::run()
{
	bool ret = m_conf.read();
	if (!ret) {
		::fprintf(stderr, "MMDVM-Host: cannot read the .ini file\n");
		return 1;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	bool m_daemon = m_conf.getDaemon();
	if (m_daemon) {
		// Create new process
		pid_t pid = ::fork();
		if (pid == -1) {
			::fprintf(stderr, "Couldn't fork() , exiting\n");
			return -1;
		} else if (pid != 0) {
			exit(EXIT_SUCCESS);
		}

		// Create new session and process group
		if (::setsid() == -1){
			::fprintf(stderr, "Couldn't setsid(), exiting\n");
			return -1;
		}

		// Set the working directory to the root directory
		if (::chdir("/") == -1){
			::fprintf(stderr, "Couldn't cd /, exiting\n");
			return -1;
		}

#if !defined(HD44780) && !defined(OLED) && !defined(_OPENWRT)
		// If we are currently root...
		if (getuid() == 0) {
			struct passwd* user = ::getpwnam("mmdvm");
			if (user == nullptr) {
				::fprintf(stderr, "Could not get the mmdvm user, exiting\n");
				return -1;
			}

			uid_t mmdvm_uid = user->pw_uid;
			gid_t mmdvm_gid = user->pw_gid;

			// Set user and group ID's to mmdvm:mmdvm
			if (::setgid(mmdvm_gid) != 0) {
				::fprintf(stderr, "Could not set mmdvm GID, exiting\n");
				return -1;
			}

			if (::setuid(mmdvm_uid) != 0) {
				::fprintf(stderr, "Could not set mmdvm UID, exiting\n");
				return -1;
			}

			// Double check it worked (AKA Paranoia)
			if (::setuid(0) != -1){
				::fprintf(stderr, "It's possible to regain root - something is wrong!, exiting\n");
				return -1;
			}
		}
	}
#else
	::fprintf(stderr, "Dropping root permissions in daemon mode is disabled.\n");
	}
#endif
#endif
	::LogInitialise(m_conf.getLogDisplayLevel(), m_conf.getLogMQTTLevel());

	std::vector<std::pair<std::string, void (*)(const unsigned char*, unsigned int)>> subscriptions;
	subscriptions.push_back(std::make_pair("display-in", CMMDVMHost::onDisplay));

	if (m_conf.getRemoteControlEnabled())
		subscriptions.push_back(std::make_pair("command", CMMDVMHost::onCommand));

	m_mqtt = new CMQTTConnection(m_conf.getMQTTHost(), m_conf.getMQTTPort(), m_conf.getMQTTName(), m_conf.getMQTTAuthEnabled(), m_conf.getMQTTUsername(), m_conf.getMQTTPassword(), subscriptions, m_conf.getMQTTKeepalive());
	ret = m_mqtt->open();
	if (!ret) {
		::fprintf(stderr, "MMDVM-Host: unable to start the MQTT Publisher\n");
		delete m_mqtt;
		m_mqtt = nullptr;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	if (m_daemon) {
		::close(STDIN_FILENO);
		::close(STDOUT_FILENO);
	}
#endif

	LogInfo(HEADER1);
	LogInfo(HEADER2);
	LogInfo(HEADER3);
	LogInfo(HEADER4);

	LogInfo("MMDVM-Host-%s is starting", VERSION);
	LogInfo("Built %s %s (GitID #%.7s)", __TIME__, __DATE__, gitversion);

	writeJSONMessage("MMDVM-Host is starting");

	readParams();

	ret = createModem();
	if (!ret)
		return 1;

#if defined(USE_DSTAR)
	if (m_dstarEnabled && !m_modem->hasDStar()) {
		LogWarning("D-Star enabled in the host but not in the modem firmware, disabling");
		m_dstarEnabled = false;
	}
#endif

#if defined(USE_DMR)
	if (m_dmrEnabled && !m_modem->hasDMR()) {
		LogWarning("DMR enabled in the host but not in the modem firmware, disabling");
		m_dmrEnabled = false;
	}
#endif

#if defined(USE_YSF)
	if (m_ysfEnabled && !m_modem->hasYSF()) {
		LogWarning("YSF enabled in the host but not in the modem firmware, disabling");
		m_ysfEnabled = false;
	}
#endif

#if defined(USE_P25)
	if (m_p25Enabled && !m_modem->hasP25()) {
		LogWarning("P25 enabled in the host but not in the modem firmware, disabling");
		m_p25Enabled = false;
	}
#endif

#if defined(USE_NXDN)
	if (m_nxdnEnabled && !m_modem->hasNXDN()) {
		LogWarning("NXDN enabled in the host but not in the modem firmware, disabling");
		m_nxdnEnabled = false;
	}
#endif

#if defined(USE_FM)
	if (m_fmEnabled && !m_modem->hasFM()) {
		LogWarning("FM enabled in the host but not in the modem firmware, disabling");
		m_fmEnabled = false;
	}
#endif

#if defined(USE_POCSAG)
	if (m_pocsagEnabled && !m_modem->hasPOCSAG()) {
		LogWarning("POCSAG enabled in the host but not in the modem firmware, disabling");
		m_pocsagEnabled = false;
	}
#endif

	LogInfo("Opening network connections");
	writeJSONMessage("Opening network connections");

#if defined(USE_DSTAR)
	if (m_dstarEnabled && m_conf.getDStarNetworkEnabled()) {
		ret = createDStarNetwork();
		if (!ret)
			return 1;
	}
#endif

#if defined(USE_DMR)
	if (m_dmrEnabled && m_conf.getDMRNetworkEnabled()) {
		ret = createDMRNetwork();
		if (!ret)
			return 1;
	}
#endif

#if defined(USE_YSF)
	if (m_ysfEnabled && m_conf.getFusionNetworkEnabled()) {
		ret = createYSFNetwork();
		if (!ret)
			return 1;
	}
#endif

#if defined(USE_P25)
	if (m_p25Enabled && m_conf.getP25NetworkEnabled()) {
		ret = createP25Network();
		if (!ret)
			return 1;
	}
#endif

#if defined(USE_NXDN)
	if (m_nxdnEnabled && m_conf.getNXDNNetworkEnabled()) {
		ret = createNXDNNetwork();
		if (!ret)
			return 1;
	}
#endif

#if defined(USE_POCSAG)
	if (m_pocsagEnabled && m_conf.getPOCSAGNetworkEnabled()) {
		ret = createPOCSAGNetwork();
		if (!ret)
			return 1;
	}
#endif

#if defined(USE_FM)
	if (m_fmEnabled && m_conf.getFMNetworkEnabled()) {
		ret = createFMNetwork();
		if (!ret)
			return 1;
	}
#endif

	sockaddr_storage transparentAddress;
	unsigned int transparentAddrLen;
	CUDPSocket* transparentSocket = nullptr;

	unsigned int sendFrameType = 0U;
	if (m_conf.getTransparentEnabled()) {
		std::string remoteAddress = m_conf.getTransparentRemoteAddress();
		unsigned short remotePort = m_conf.getTransparentRemotePort();
		unsigned short localPort  = m_conf.getTransparentLocalPort();
		sendFrameType             = m_conf.getTransparentSendFrameType();

		LogInfo("Transparent Data");
		LogInfo("    Remote Address: %s", remoteAddress.c_str());
		LogInfo("    Remote Port: %hu", remotePort);
		LogInfo("    Local Port: %hu", localPort);
		LogInfo("    Send Frame Type: %u", sendFrameType);

		if (CUDPSocket::lookup(remoteAddress, remotePort, transparentAddress, transparentAddrLen) != 0) {
			LogError("Unable to resolve the address of the Transparent Data source");
			return 1;
		}

		transparentSocket = new CUDPSocket(localPort);
		ret = transparentSocket->open(transparentAddress);
		if (!ret) {
			LogWarning("Could not open the Transparent data socket, disabling");
			delete transparentSocket;
			transparentSocket = nullptr;
			sendFrameType=0;
		}
		m_modem->setTransparentDataParams(sendFrameType);
	}

	if (m_conf.getLockFileEnabled()) {
		m_lockFileEnabled = true;
		m_lockFileName    = m_conf.getLockFileName();

		LogInfo("Lock File Parameters");
		LogInfo("    Name: %s", m_lockFileName.c_str());

		removeLockFile();
	}

	if (m_conf.getCWIdEnabled()) {
		unsigned int time = m_conf.getCWIdTime();
		m_cwCallsign      = m_conf.getCWIdCallsign();

		LogInfo("CW Id Parameters");
		LogInfo("    Time: %u mins", time);
		LogInfo("    Callsign: %s", m_cwCallsign.c_str());

		m_cwIdTime = time * 60U;

		m_cwIdTimer.setTimeout(m_cwIdTime / 4U);
		m_cwIdTimer.start();
	}

	// For all modes we handle RSSI
	std::string rssiMappingFile = m_conf.getModemRSSIMappingFile();

	CRSSIInterpolator* rssi = new CRSSIInterpolator;
	if (!rssiMappingFile.empty()) {
		LogInfo("RSSI");
		LogInfo("    Mapping File: %s", rssiMappingFile.c_str());
		rssi->load(rssiMappingFile);
	}

#if defined(USE_DMR) || defined(USE_P25)
	// For DMR and P25 we try to map IDs to callsigns
	if (m_dmrEnabled || m_p25Enabled) {
		std::string lookupFile  = m_conf.getDMRIdLookupFile();
		unsigned int reloadTime = m_conf.getDMRIdLookupTime();

		LogInfo("DMR Id Lookups");
		LogInfo("    File: %s", lookupFile.length() > 0U ? lookupFile.c_str() : "None");
		if (reloadTime > 0U)
			LogInfo("    Reload: %u hours", reloadTime);

		m_dmrLookup = new CDMRLookup(lookupFile, reloadTime);
		m_dmrLookup->read();
	}
#endif

	LogInfo("Starting protocol handlers");
	writeJSONMessage("Starting protocol handlers");

	CStopWatch stopWatch;
	stopWatch.start();

#if defined(USE_DSTAR)
	if (m_dstarEnabled) {
		std::string module                 = m_conf.getDStarModule();
		bool selfOnly                      = m_conf.getDStarSelfOnly();
		std::vector<std::string> blackList = m_conf.getDStarBlackList();
		std::vector<std::string> whiteList = m_conf.getDStarWhiteList();
		bool ackReply                      = m_conf.getDStarAckReply();
		unsigned int ackTime               = m_conf.getDStarAckTime();
		DSTAR_ACK ackMessage               = m_conf.getDStarAckMessage();
		bool errorReply                    = m_conf.getDStarErrorReply();
		bool remoteGateway                 = m_conf.getDStarRemoteGateway();
		m_dstarRFModeHang                  = m_conf.getDStarModeHang();

		LogInfo("D-Star RF Parameters");
		LogInfo("    Module: %s", module.c_str());
		LogInfo("    Self Only: %s", selfOnly ? "yes" : "no");
		LogInfo("    Ack Reply: %s", ackReply ? "yes" : "no");
		LogInfo("    Ack message: %s", ackMessage == DSTAR_ACK::RSSI? "RSSI" : (ackMessage == DSTAR_ACK::SMETER ? "SMETER" : "BER"));
		LogInfo("    Ack Time: %ums", ackTime);
		LogInfo("    Error Reply: %s", errorReply ? "yes" : "no");
		LogInfo("    Remote Gateway: %s", remoteGateway ? "yes" : "no");
		LogInfo("    Mode Hang: %us", m_dstarRFModeHang);

		if (blackList.size() > 0U)
			LogInfo("    Black List: %u", blackList.size());
		if (whiteList.size() > 0U)
			LogInfo("    White List: %u", whiteList.size());

		m_dstar = new CDStarControl(m_callsign, module, selfOnly, ackReply, ackTime, ackMessage, errorReply, blackList, whiteList, m_dstarNetwork, m_timeout, m_duplex, remoteGateway, rssi);
	}
#endif

#if defined(USE_DMR)
	DMR_BEACONS dmrBeacons = DMR_BEACONS::OFF;
	CTimer dmrBeaconIntervalTimer(1000U);
	CTimer dmrBeaconDurationTimer(1000U);

	if (m_dmrEnabled) {
		unsigned int id             = m_conf.getDMRId();
		unsigned int colorCode      = m_conf.getDMRColorCode();
		bool selfOnly               = m_conf.getDMRSelfOnly();
		bool embeddedLCOnly         = m_conf.getDMREmbeddedLCOnly();
		bool dumpTAData             = m_conf.getDMRDumpTAData();
		std::vector<unsigned int> prefixes  = m_conf.getDMRPrefixes();
		std::vector<unsigned int> blackList = m_conf.getDMRBlackList();
		std::vector<unsigned int> whiteList = m_conf.getDMRWhiteList();
		std::vector<unsigned int> slot1TGWhiteList = m_conf.getDMRSlot1TGWhiteList();
		std::vector<unsigned int> slot2TGWhiteList = m_conf.getDMRSlot2TGWhiteList();
		unsigned int callHang       = m_conf.getDMRCallHang();
		unsigned int txHang         = m_conf.getDMRTXHang();
		unsigned int jitter         = m_conf.getDMRNetworkJitter();
		m_dmrRFModeHang             = m_conf.getDMRModeHang();
		dmrBeacons                  = m_conf.getDMRBeacons();
		DMR_OVCM ovcm               = m_conf.getDMROVCM();
		bool protect                = m_conf.getDMRProtect();

		bool trunking               = m_conf.getDMRTrunkingEnabled();
		bool controlChannel         = m_conf.getDMRControlChannel();
		unsigned int systemCode     = m_conf.getDMRSystemCode();
		bool registrationRequired   = m_conf.getDMRRegistrationRequired();
		bool alternateSlot          = m_conf.getDMRControlChannelAlternateSlot();

		if (txHang > m_dmrRFModeHang)
			txHang = m_dmrRFModeHang;

		if (m_conf.getDMRNetworkEnabled()) {
			if (txHang > m_dmrNetModeHang)
				txHang = m_dmrNetModeHang;
		}

		if (callHang > txHang)
			callHang = txHang;

		LogInfo("DMR RF Parameters");
		LogInfo("    Id: %u", id);
		LogInfo("    Color Code: %u", colorCode);
		LogInfo("    Self Only: %s", selfOnly ? "yes" : "no");
		LogInfo("    Embedded LC Only: %s", embeddedLCOnly ? "yes" : "no");
		LogInfo("    Dump Talker Alias Data: %s", dumpTAData ? "yes" : "no");
		LogInfo("    Prefixes: %u", prefixes.size());

		if (blackList.size() > 0U)
			LogInfo("    Source ID Black List: %u", blackList.size());
		if (whiteList.size() > 0U)
			LogInfo("    Source ID White List: %u", whiteList.size());
		if (slot1TGWhiteList.size() > 0U)
			LogInfo("    Slot 1 TG White List: %u", slot1TGWhiteList.size());
		if (slot2TGWhiteList.size() > 0U)
			LogInfo("    Slot 2 TG White List: %u", slot2TGWhiteList.size());

		LogInfo("    Call Hang: %us", callHang);
		LogInfo("    Duplex Control TG: %u", m_conf.getDMRDuplexControlTG());
		LogInfo("    TX Hang: %us", txHang);
		LogInfo("    Mode Hang: %us", m_dmrRFModeHang);
		if (ovcm == DMR_OVCM::OFF)
			LogInfo("    OVCM: off");
		else if (ovcm == DMR_OVCM::RX_ON)
			LogInfo("    OVCM: on(rx only)");
		else if (ovcm == DMR_OVCM::TX_ON)
			LogInfo("    OVCM: on(tx only)");
		else if (ovcm == DMR_OVCM::ON)
			LogInfo("    OVCM: on");
		else if (ovcm == DMR_OVCM::FORCE_OFF)
			LogInfo("    OVCM: off (forced)");

		if (protect)
			LogInfo("    Protect: yes");

		if (trunking) {
			LogInfo("    Trunking: yes");
			LogInfo("    Control Channel: %s", controlChannel ? "yes" : "no");
			LogInfo("    System Code: %u", systemCode);
			LogInfo("    Registration Required: %s", registrationRequired ? "yes" : "no");
			LogInfo("    Alternate Slot: %s", alternateSlot ? "yes" : "no");
		} else {
			LogInfo("    Trunking: no");
		}

		switch (dmrBeacons) {
			case DMR_BEACONS::NETWORK: {
					unsigned int dmrBeaconDuration = m_conf.getDMRBeaconDuration();

					LogInfo("    DMR Roaming Beacons Type: network");
					LogInfo("    DMR Roaming Beacons Duration: %us", dmrBeaconDuration);

					dmrBeaconDurationTimer.setTimeout(dmrBeaconDuration);
				}
				break;
			case DMR_BEACONS::TIMED: {
					unsigned int dmrBeaconInterval = m_conf.getDMRBeaconInterval();
					unsigned int dmrBeaconDuration = m_conf.getDMRBeaconDuration();

					LogInfo("    DMR Roaming Beacons Type: timed");
					LogInfo("    DMR Roaming Beacons Interval: %us", dmrBeaconInterval);
					LogInfo("    DMR Roaming Beacons Duration: %us", dmrBeaconDuration);

					dmrBeaconDurationTimer.setTimeout(dmrBeaconDuration);

					dmrBeaconIntervalTimer.setTimeout(dmrBeaconInterval);
					dmrBeaconIntervalTimer.start();
				}
				break;
			default:
				LogInfo("    DMR Roaming Beacons Type: off");
				break;
		}

		m_dmr = new CDMRControl(id, colorCode, callHang, selfOnly, embeddedLCOnly, dumpTAData, prefixes, blackList, whiteList, slot1TGWhiteList, slot2TGWhiteList, m_timeout, m_modem, m_dmrNetwork, m_duplex, m_dmrLookup, rssi, jitter, ovcm, protect, m_conf.getDMRDuplexControlTG());

		m_dmrTXTimer.setTimeout(txHang);

		// Tier III options
		if (trunking) {
			m_modem->setDMRShortLC(systemCode, controlChannel, registrationRequired);

			setMode(MODE_DMR);

			if (controlChannel)
				m_modem->writeDMRAloha(systemCode, registrationRequired, alternateSlot);
		}
	}
#endif

#if defined(USE_YSF)
	if (m_ysfEnabled) {
		bool lowDeviation   = m_conf.getFusionLowDeviation();
		bool remoteGateway  = m_conf.getFusionRemoteGateway();
		unsigned int txHang = m_conf.getFusionTXHang();
		bool selfOnly       = m_conf.getFusionSelfOnly();
		m_ysfRFModeHang     = m_conf.getFusionModeHang();

		LogInfo("YSF RF Parameters");
		LogInfo("    Low Deviation: %s", lowDeviation ? "yes" : "no");
		LogInfo("    Remote Gateway: %s", remoteGateway ? "yes" : "no");
		LogInfo("    TX Hang: %us", txHang);
		LogInfo("    Self Only: %s", selfOnly ? "yes" : "no");
		LogInfo("    Mode Hang: %us", m_ysfRFModeHang);

		m_ysf = new CYSFControl(m_callsign, selfOnly, m_ysfNetwork, m_timeout, m_duplex, lowDeviation, remoteGateway, rssi);
	}
#endif

#if defined(USE_P25)
	if (m_p25Enabled) {
		unsigned int id     = m_conf.getP25Id();
		unsigned int nac    = m_conf.getP25NAC();
		unsigned int txHang = m_conf.getP25TXHang();
		bool uidOverride    = m_conf.getP25OverrideUID();
		bool selfOnly       = m_conf.getP25SelfOnly();
		bool remoteGateway  = m_conf.getP25RemoteGateway();
		bool softId         = m_conf.getP25SoftId();
		m_p25RFModeHang     = m_conf.getP25ModeHang();

		LogInfo("P25 RF Parameters");
		LogInfo("    Id: %u", id);
		LogInfo("    NAC: $%03X", nac);
		LogInfo("    UID Override: %s", uidOverride ? "yes" : "no");
		LogInfo("    Self Only: %s", selfOnly ? "yes" : "no");
		LogInfo("    Remote Gateway: %s", remoteGateway ? "yes" : "no");
		LogInfo("    Soft ID: %s", softId ? "yes" : "no");
		LogInfo("    TX Hang: %us", txHang);
		LogInfo("    Mode Hang: %us", m_p25RFModeHang);

		m_p25 = new CP25Control(nac, id, selfOnly, uidOverride, m_p25Network, m_timeout, m_duplex, m_dmrLookup, remoteGateway, softId, rssi);
	}
#endif

#if defined(USE_NXDN)
	if (m_nxdnEnabled) {
		std::string lookupFile  = m_conf.getNXDNIdLookupFile();
		unsigned int reloadTime = m_conf.getNXDNIdLookupTime();

		LogInfo("NXDN Id Lookups");
		LogInfo("    File: %s", lookupFile.length() > 0U ? lookupFile.c_str() : "None");
		if (reloadTime > 0U)
			LogInfo("    Reload: %u hours", reloadTime);

		m_nxdnLookup = new CNXDNLookup(lookupFile, reloadTime);
		m_nxdnLookup->read();

		unsigned int id     = m_conf.getNXDNId();
		unsigned int ran    = m_conf.getNXDNRAN();
		bool selfOnly       = m_conf.getNXDNSelfOnly();
		bool remoteGateway  = m_conf.getNXDNRemoteGateway();
		unsigned int txHang = m_conf.getNXDNTXHang();
		m_nxdnRFModeHang    = m_conf.getNXDNModeHang();

		LogInfo("NXDN RF Parameters");
		LogInfo("    Id: %u", id);
		LogInfo("    RAN: %u", ran);
		LogInfo("    Self Only: %s", selfOnly ? "yes" : "no");
		LogInfo("    Remote Gateway: %s", remoteGateway ? "yes" : "no");
		LogInfo("    TX Hang: %us", txHang);
		LogInfo("    Mode Hang: %us", m_nxdnRFModeHang);

		m_nxdn = new CNXDNControl(ran, id, selfOnly, m_nxdnNetwork, m_timeout, m_duplex, remoteGateway, m_nxdnLookup, rssi);
	}
#endif

#if defined(USE_POCSAG)
	CTimer pocsagTimer(1000U, 30U);

	if (m_pocsagEnabled) {
		unsigned int frequency = m_conf.getPOCSAGFrequency();

		LogInfo("POCSAG RF Parameters");
		LogInfo("    Frequency: %uHz", frequency);

		m_pocsag = new CPOCSAGControl(m_pocsagNetwork);

		if (m_pocsagNetwork != nullptr)
			pocsagTimer.start();
	}
#endif

#if defined(USE_FM)
	if (m_fmEnabled) {
		bool preEmphasis  = m_conf.getFMPreEmphasis();
		bool deEmphasis   = m_conf.getFMDeEmphasis();
		float txAudioGain = m_conf.getFMTXAudioGain();
		float rxAudioGain = m_conf.getFMRXAudioGain();
		m_fmRFModeHang    = m_conf.getFMModeHang();

		m_fm = new CFMControl(m_fmNetwork, txAudioGain, rxAudioGain, preEmphasis, deEmphasis, rssi);
	}
#endif

	bool remoteControlEnabled = m_conf.getRemoteControlEnabled();
	if (remoteControlEnabled)
		m_remoteControl = new CRemoteControl(this, m_mqtt);

	if (!m_conf.getDMRTrunkingEnabled())
		setMode(MODE_IDLE);

	while (!m_killed) {
		bool lockout = m_modem->hasLockout();

		if (lockout && m_mode != MODE_LOCKOUT)
			setMode(MODE_LOCKOUT);
		else if (!lockout && m_mode == MODE_LOCKOUT)
			setMode(MODE_IDLE);

		bool error = m_modem->hasError();
		if (error && m_mode != MODE_ERROR)
			setMode(MODE_ERROR);
		else if (!error && m_mode == MODE_ERROR)
			setMode(MODE_IDLE);

		unsigned char data[500U];
		unsigned int len;
		bool ret;

#if defined(USE_DSTAR)
		len = m_modem->readDStarData(data);
		if (m_dstar != nullptr && m_dstarEnabled && len > 0U) {
			if (m_mode == MODE_IDLE) {
				bool ret = m_dstar->writeModem(data, len);
				if (ret) {
					m_modeTimer.setTimeout(m_dstarRFModeHang);
					setMode(MODE_DSTAR);
				}
			} else if (m_mode == MODE_DSTAR) {
				bool ret = m_dstar->writeModem(data, len);
				if (ret)
					m_modeTimer.start();
			} else if (m_mode != MODE_LOCKOUT) {
				LogWarning("D-Star modem data received when in mode %u", m_mode);
			}
		}
#endif

#if defined(USE_DMR)
		len = m_modem->readDMRData1(data);
		if (m_dmr != nullptr && m_dmrEnabled && len > 0U) {
			if (m_mode == MODE_IDLE) {
				if (m_duplex) {
					bool ret = m_dmr->processWakeup(data);
					if (ret) {
						m_modeTimer.setTimeout(m_dmrRFModeHang);
						setMode(MODE_DMR);
						dmrBeaconDurationTimer.stop();
					}
				} else {
					m_modeTimer.setTimeout(m_dmrRFModeHang);
					setMode(MODE_DMR);
					m_dmr->writeModemSlot1(data, len);
					dmrBeaconDurationTimer.stop();
				}
			} else if (m_mode == MODE_DMR) {
				if (m_duplex && !m_modem->hasTX()) {
					bool ret = m_dmr->processWakeup(data);
					if (ret) {
						m_modem->writeDMRStart(true);
						m_dmrTXTimer.start();
					}
				} else {
					bool ret = m_dmr->writeModemSlot1(data, len);
					if (ret) {
						dmrBeaconDurationTimer.stop();
						m_modeTimer.start();
						if (m_duplex)
							m_dmrTXTimer.start();
					}
				}
			} else if (m_mode != MODE_LOCKOUT) {
				LogWarning("DMR modem data received when in mode %u", m_mode);
			}
		}

		len = m_modem->readDMRData2(data);
		if (m_dmr != nullptr && m_dmrEnabled && len > 0U) {
			if (m_mode == MODE_IDLE) {
				if (m_duplex) {
					bool ret = m_dmr->processWakeup(data);
					if (ret || m_conf.getDMRTrunkingEnabled()) {
						m_modeTimer.setTimeout(m_dmrRFModeHang);
						setMode(MODE_DMR);
						dmrBeaconDurationTimer.stop();
					}
				} else {
					m_modeTimer.setTimeout(m_dmrRFModeHang);
					setMode(MODE_DMR);
					m_dmr->writeModemSlot2(data, len);
					dmrBeaconDurationTimer.stop();
				}
			} else if (m_mode == MODE_DMR) {
				if (m_duplex && !m_modem->hasTX()) {
					bool ret = m_dmr->processWakeup(data);
					if (ret || m_conf.getDMRTrunkingEnabled()) {
						m_modem->writeDMRStart(true);
						m_dmrTXTimer.start();
					}
				} else {
					bool ret = m_dmr->writeModemSlot2(data, len);
					if (ret) {
						dmrBeaconDurationTimer.stop();
						m_modeTimer.start();
						if (m_duplex)
							m_dmrTXTimer.start();
					}
				}
			} else if (m_mode != MODE_LOCKOUT) {
				LogWarning("DMR modem data received when in mode %u", m_mode);
			}
		}
#endif

#if defined(USE_YSF)
		len = m_modem->readYSFData(data);
		if (m_ysf != nullptr && m_ysfEnabled && len > 0U) {
			if (m_mode == MODE_IDLE) {
				bool ret = m_ysf->writeModem(data, len);
				if (ret) {
					m_modeTimer.setTimeout(m_ysfRFModeHang);
					setMode(MODE_YSF);
				}
			} else if (m_mode == MODE_YSF) {
				bool ret = m_ysf->writeModem(data, len);
				if (ret)
					m_modeTimer.start();
			} else if (m_mode != MODE_LOCKOUT) {
				LogWarning("System Fusion modem data received when in mode %u", m_mode);
			}
		}
#endif

#if defined(USE_P25)
		len = m_modem->readP25Data(data);
		if (m_p25 != nullptr && m_p25Enabled && len > 0U) {
			if (m_mode == MODE_IDLE) {
				bool ret = m_p25->writeModem(data, len);
				if (ret) {
					m_modeTimer.setTimeout(m_p25RFModeHang);
					setMode(MODE_P25);
				}
			} else if (m_mode == MODE_P25) {
				bool ret = m_p25->writeModem(data, len);
				if (ret)
					m_modeTimer.start();
			} else if (m_mode != MODE_LOCKOUT) {
				LogWarning("P25 modem data received when in mode %u", m_mode);
			}
		}
#endif

#if defined(USE_NXDN)
		len = m_modem->readNXDNData(data);
		if (m_nxdn != nullptr && m_nxdnEnabled && len > 0U) {
			if (m_mode == MODE_IDLE) {
				bool ret = m_nxdn->writeModem(data, len);
				if (ret) {
					m_modeTimer.setTimeout(m_nxdnRFModeHang);
					setMode(MODE_NXDN);
				}
			} else if (m_mode == MODE_NXDN) {
				bool ret = m_nxdn->writeModem(data, len);
				if (ret)
					m_modeTimer.start();
			} else if (m_mode != MODE_LOCKOUT) {
				LogWarning("NXDN modem data received when in mode %u", m_mode);
			}
		}
#endif

#if defined(USE_FM)
		len = m_modem->readFMData(data);
		if (m_fm != nullptr && m_fmEnabled && len > 0U) {
			if (m_mode == MODE_IDLE) {
				bool ret = m_fm->writeModem(data, len);
				if (ret) {
					m_modeTimer.setTimeout(m_fmRFModeHang);
					setMode(MODE_FM);
				}
			} else if (m_mode == MODE_FM) {
				bool ret = m_fm->writeModem(data, len);
				if (ret)
					m_modeTimer.start();
			} else if (m_mode != MODE_LOCKOUT) {
				LogWarning("FM modem data received when in mode %u", m_mode);
			}
		}
#endif

		len = m_modem->readTransparentData(data);
		if (transparentSocket != nullptr && len > 0U)
			transparentSocket->write(data, len, transparentAddress, transparentAddrLen);

		if (!m_fixedMode && !m_conf.getDMRTrunkingEnabled()) {
			if (m_modeTimer.isRunning() && m_modeTimer.hasExpired())
				setMode(MODE_IDLE);
		}

#if defined(USE_DSTAR)
		if (m_dstar != nullptr) {
			ret = m_modem->hasDStarSpace();
			if (ret) {
				len = m_dstar->readModem(data);
				if (len > 0U) {
					if (m_mode == MODE_IDLE) {
						m_modeTimer.setTimeout(m_dstarNetModeHang);
						setMode(MODE_DSTAR);
					}
					if (m_mode == MODE_DSTAR) {
						m_modem->writeDStarData(data, len);
						m_modeTimer.start();
					} else if (m_mode != MODE_LOCKOUT) {
						LogWarning("D-Star data received when in mode %u", m_mode);
					}
				}
			}
		}
#endif

#if defined(USE_DMR)
		if (m_dmr != nullptr) {
			ret = m_modem->hasDMRSpace1();
			if (ret) {
				len = m_dmr->readModemSlot1(data);
				if (len > 0U) {
					if (m_mode == MODE_IDLE) {
						m_modeTimer.setTimeout(m_dmrNetModeHang);
						setMode(MODE_DMR);
					}
					if (m_mode == MODE_DMR) {
						if (m_duplex) {
							m_modem->writeDMRStart(true);
							m_dmrTXTimer.start();
						}
						m_modem->writeDMRData1(data, len);
						dmrBeaconDurationTimer.stop();
						m_modeTimer.start();
					} else if (m_mode != MODE_LOCKOUT) {
						LogWarning("DMR data received when in mode %u", m_mode);
					}
				}
			}

			ret = m_modem->hasDMRSpace2();
			if (ret) {
				len = m_dmr->readModemSlot2(data);
				if (len > 0U) {
					if (m_mode == MODE_IDLE) {
						m_modeTimer.setTimeout(m_dmrNetModeHang);
						setMode(MODE_DMR);
					}
					if (m_mode == MODE_DMR) {
						if (m_duplex) {
							m_modem->writeDMRStart(true);
							m_dmrTXTimer.start();
						}
						m_modem->writeDMRData2(data, len);
						dmrBeaconDurationTimer.stop();
						m_modeTimer.start();
					} else if (m_mode != MODE_LOCKOUT) {
						LogWarning("DMR data received when in mode %u", m_mode);
					}
				}
			}
		}
#endif

#if defined(USE_YSF)
		if (m_ysf != nullptr) {
			ret = m_modem->hasYSFSpace();
			if (ret) {
				len = m_ysf->readModem(data);
				if (len > 0U) {
					if (m_mode == MODE_IDLE) {
						m_modeTimer.setTimeout(m_ysfNetModeHang);
						setMode(MODE_YSF);
					}
					if (m_mode == MODE_YSF) {
						m_modem->writeYSFData(data, len);
						m_modeTimer.start();
					} else if (m_mode != MODE_LOCKOUT) {
						LogWarning("System Fusion data received when in mode %u", m_mode);
					}
				}
			}
		}
#endif

#if defined(USE_P25)
		if (m_p25 != nullptr) {
			ret = m_modem->hasP25Space();
			if (ret) {
				len = m_p25->readModem(data);
				if (len > 0U) {
					if (m_mode == MODE_IDLE) {
						m_modeTimer.setTimeout(m_p25NetModeHang);
						setMode(MODE_P25);
					}
					if (m_mode == MODE_P25) {
						m_modem->writeP25Data(data, len);
						m_modeTimer.start();
					} else if (m_mode != MODE_LOCKOUT) {
						LogWarning("P25 data received when in mode %u", m_mode);
					}
				}
			}
		}
#endif

#if defined(USE_NXDN)
		if (m_nxdn != nullptr) {
			ret = m_modem->hasNXDNSpace();
			if (ret) {
				len = m_nxdn->readModem(data);
				if (len > 0U) {
					if (m_mode == MODE_IDLE) {
						m_modeTimer.setTimeout(m_nxdnNetModeHang);
						setMode(MODE_NXDN);
					}
					if (m_mode == MODE_NXDN) {
						m_modem->writeNXDNData(data, len);
						m_modeTimer.start();
					} else if (m_mode != MODE_LOCKOUT) {
						LogWarning("NXDN data received when in mode %u", m_mode);
					}
				}
			}
		}
#endif

#if defined(USE_POCSAG)
		if (m_pocsag != nullptr) {
			ret = m_modem->hasPOCSAGSpace();
			if (ret) {
				len = m_pocsag->readModem(data);
				if (len > 0U) {
					if (m_mode == MODE_IDLE) {
						m_modeTimer.setTimeout(m_pocsagNetModeHang);
						setMode(MODE_POCSAG);
					}
					if (m_mode == MODE_POCSAG) {
						m_modem->writePOCSAGData(data, len);
						m_modeTimer.start();
					} else if (m_mode != MODE_LOCKOUT) {
						LogWarning("POCSAG data received when in mode %u", m_mode);
					}
				}
			}
		}
#endif

#if defined(USE_FM)
		if (m_fm != nullptr) {
			unsigned int space = m_modem->getFMSpace();
			if (space > 0U) {
				len = m_fm->readModem(data, space);
				if (len > 0U) {
					if (m_mode == MODE_IDLE) {
						m_modeTimer.setTimeout(m_fmNetModeHang);
						setMode(MODE_FM);
					}
					if (m_mode == MODE_FM) {
						m_modem->writeFMData(data, len);
						m_modeTimer.start();
					} else if (m_mode != MODE_LOCKOUT) {
						LogWarning("FM data received when in mode %u", m_mode);
					}
				}
			}
		}
#endif

		if (transparentSocket != nullptr) {
			sockaddr_storage address;
			unsigned int addrlen;
			len = transparentSocket->read(data, 200U, address, addrlen);
			if (len > 0U)
				m_modem->writeTransparentData(data, len);
		}

		len = m_modem->readSerialData(data);
		if (len > 0U)
			m_mqtt->publish("display-out", data, len);

		unsigned int ms = stopWatch.elapsed();
		stopWatch.start();

		m_modem->clock(ms);

		m_serialTimer.clock(ms);
		if (m_serialTimer.isRunning() && m_serialTimer.hasExpired()) {
			unsigned int length = m_serialLength - m_serialStart;
			if (length > 252U) {
				m_modem->writeSerialData(m_serialBuffer + m_serialStart, 252U);

				m_serialStart += 252U;
				m_serialTimer.start();
			} else {
				if (length > 0U)
					m_modem->writeSerialData(m_serialBuffer + m_serialStart, length);

				m_serialLength = 0U;
				m_serialStart  = 0U;
				m_serialTimer.stop();
			}
		}

		if (!m_fixedMode)
			m_modeTimer.clock(ms);

		if (m_reload) {
#if defined(USE_DMR) || defined(USE_P25)
			if (m_dmrLookup != nullptr)
				m_dmrLookup->reload();
#endif
#if defined(USE_NXDN)
			if (m_nxdnLookup != nullptr)
				m_nxdnLookup->reload();
#endif
			m_reload = false;
		}

#if defined(USE_DSTAR)
		if (m_dstar != nullptr)
			m_dstar->clock();
#endif
#if defined(USE_DMR)
		if (m_dmr != nullptr)
			m_dmr->clock();
#endif
#if defined(USE_YSF)
		if (m_ysf != nullptr)
			m_ysf->clock(ms);
#endif
#if defined(USE_P25)
		if (m_p25 != nullptr)
			m_p25->clock(ms);
#endif
#if defined(USE_NXDN)
		if (m_nxdn != nullptr)
			m_nxdn->clock(ms);
#endif
#if defined(USE_POCSAG)
		if (m_pocsag != nullptr)
			m_pocsag->clock(ms);
#endif
#if defined(USE_FM)
		if (m_fm != nullptr)
			m_fm->clock(ms);
#endif

#if defined(USE_DSTAR)
		if (m_dstarNetwork != nullptr)
			m_dstarNetwork->clock(ms);
#endif
#if defined(USE_DMR)
		if (m_dmrNetwork != nullptr)
			m_dmrNetwork->clock(ms);
#endif
#if defined(USE_YSF)
		if (m_ysfNetwork != nullptr)
			m_ysfNetwork->clock(ms);
#endif
#if defined(USE_P25)
		if (m_p25Network != nullptr)
			m_p25Network->clock(ms);
#endif
#if defined(USE_NXDN)
		if (m_nxdnNetwork != nullptr)
			m_nxdnNetwork->clock(ms);
#endif
#if defined(USE_POCSAG)
		if (m_pocsagNetwork != nullptr)
			m_pocsagNetwork->clock(ms);
#endif
#if defined(USE_FM)
		if (m_fmNetwork != nullptr)
			m_fmNetwork->clock(ms);
#endif
		m_cwIdTimer.clock(ms);
		if (m_cwIdTimer.isRunning() && m_cwIdTimer.hasExpired()) {
			if (!m_modem->hasTX()){
				LogDebug("sending CW ID");
				writeJSONMessage("Sending CW ID");
				m_modem->sendCWId(m_cwCallsign);

				m_cwIdTimer.setTimeout(m_cwIdTime);
				m_cwIdTimer.start();
			}
		}

#if defined(USE_DMR)
		switch (dmrBeacons) {
			case DMR_BEACONS::TIMED:
				dmrBeaconIntervalTimer.clock(ms);
				if (dmrBeaconIntervalTimer.isRunning() && dmrBeaconIntervalTimer.hasExpired()) {
					if ((m_mode == MODE_IDLE || m_mode == MODE_DMR) && !m_modem->hasTX()) {
						if (!m_fixedMode && m_mode == MODE_IDLE)
							setMode(MODE_DMR);
						dmrBeaconIntervalTimer.start();
						dmrBeaconDurationTimer.start();
						if (m_duplex)
							LogDebug("sending DMR Roaming Beacon (timed mode)");
					}
				}
				break;
			case DMR_BEACONS::NETWORK:
				if (m_dmrNetwork != nullptr && m_dmrEnabled) {
					bool beacon = m_dmrNetwork->wantsBeacon();
					if (beacon) {
						if ((m_mode == MODE_IDLE || m_mode == MODE_DMR) && !m_modem->hasTX()) {
							if (!m_fixedMode && m_mode == MODE_IDLE)
								setMode(MODE_DMR);
							dmrBeaconDurationTimer.start();
							if (m_duplex)
								LogDebug("sending DMR Roaming Beacon (network mode)");
						}
					}
				}
				break;
			default:
				break;
		}

		dmrBeaconDurationTimer.clock(ms);
		if (dmrBeaconDurationTimer.isRunning() && dmrBeaconDurationTimer.hasExpired()) {
			if (!m_fixedMode && !m_conf.getDMRTrunkingEnabled())
				setMode(MODE_IDLE);
			dmrBeaconDurationTimer.stop();
		}

		m_dmrTXTimer.clock(ms);
		if (m_dmrTXTimer.isRunning() && m_dmrTXTimer.hasExpired()) {
			if (m_conf.getDMRTrunkingEnabled()) {
				if (!m_conf.getDMRControlChannel())
					m_modem->writeDMRStart(false);
			} else {
				m_modem->writeDMRStart(false);
			}
			m_dmrTXTimer.stop();
		}
#endif

#if defined(USE_POCSAG)
		pocsagTimer.clock(ms);
		if (pocsagTimer.isRunning() && pocsagTimer.hasExpired()) {
			assert(m_pocsagNetwork != nullptr);
			m_pocsagNetwork->enable((m_mode == MODE_IDLE || m_mode == MODE_POCSAG) && m_pocsagEnabled);
			pocsagTimer.start();
		}
#endif

		if (ms < 5U)
			CThread::sleep(5U);
	}

	LogInfo("MMDVM-Host is stopping");
	writeJSONMessage("MMDVM-Host is stopping");

	setMode(MODE_QUIT);

#if defined(USE_DMR) || defined(USE_P25)
	if (m_dmrLookup != nullptr)
		m_dmrLookup->stop();
#endif
#if defined(USE_NXDN)
	if (m_nxdnLookup != nullptr)
		m_nxdnLookup->stop();
#endif

	LogInfo("Closing network connections");
	writeJSONMessage("Closing network connections");

#if defined(USE_DSTAR)
	if (m_dstarNetwork != nullptr) {
		m_dstarNetwork->close();
		delete m_dstarNetwork;
	}
#endif

#if defined(USE_DMR)
	if (m_dmrNetwork != nullptr) {
		m_dmrNetwork->close();
		delete m_dmrNetwork;
	}
#endif

#if defined(USE_YSF)
	if (m_ysfNetwork != nullptr) {
		m_ysfNetwork->close();
		delete m_ysfNetwork;
	}
#endif

#if defined(USE_P25)
	if (m_p25Network != nullptr) {
		m_p25Network->close();
		delete m_p25Network;
	}
#endif

#if defined(USE_NXDN)
	if (m_nxdnNetwork != nullptr) {
		m_nxdnNetwork->close();
		delete m_nxdnNetwork;
	}
#endif

#if defined(USE_POCSAG)
	if (m_pocsagNetwork != nullptr) {
		m_pocsagNetwork->close();
		delete m_pocsagNetwork;
	}
#endif

#if defined(USE_FM)
	if (m_fmNetwork != nullptr) {
		m_fmNetwork->close();
		delete m_fmNetwork;
	}
#endif

	if (transparentSocket != nullptr) {
		transparentSocket->close();
		delete transparentSocket;
	}

	delete m_remoteControl;

	LogInfo("Stopping protocol handlers");
	writeJSONMessage("Stopping protocol handlers");

#if defined(USE_DSTAR)
	delete m_dstar;
#endif
#if defined(USE_DMR)
	delete m_dmr;
#endif
#if defined(USE_YSF)
	delete m_ysf;
#endif
#if defined(USE_P25)
	delete m_p25;
#endif
#if defined(USE_NXDN)
	delete m_nxdn;
#endif
#if defined(USE_POCSAG)
	delete m_pocsag;
#endif
#if defined(USE_FM)
	delete m_fm;
#endif

	LogInfo("MMDVM-Host-%s has stopped", VERSION);
	writeJSONMessage("MMDVM-Host has stopped");

	m_modem->close();
	delete m_modem;

	if (m_mqtt != nullptr) {
		m_mqtt->close();
		delete m_mqtt;
		m_mqtt = nullptr;
	}

	return 0;
}

bool CMMDVMHost::createModem()
{
	std::string protocol         = m_conf.getModemProtocol();
	std::string uartPort         = m_conf.getModemUARTPort();
	unsigned int uartSpeed       = m_conf.getModemUARTSpeed();
	std::string i2cPort          = m_conf.getModemI2CPort();
#if defined(__linux__)
	unsigned int i2cAddress      = m_conf.getModemI2CAddress();
#endif
	std::string modemAddress     = m_conf.getModemModemAddress();
	unsigned short modemPort     = m_conf.getModemModemPort();
	std::string localAddress     = m_conf.getModemLocalAddress();
	unsigned short localPort     = m_conf.getModemLocalPort();
	bool rxInvert                = m_conf.getModemRXInvert();
	bool txInvert                = m_conf.getModemTXInvert();
	bool pttInvert               = m_conf.getModemPTTInvert();
	unsigned int txDelay         = m_conf.getModemTXDelay();
#if defined(USE_DMR)
	unsigned int dmrDelay        = m_conf.getModemDMRDelay();
#else
	unsigned int dmrDelay        = 0U;
#endif
	float rxLevel                = m_conf.getModemRXLevel();
	float cwIdTXLevel            = m_conf.getModemCWIdTXLevel();
#if defined(USE_DSTAR)
	float dstarTXLevel           = m_conf.getModemDStarTXLevel();
#else
	float dstarTXLevel           = 0.0F;
#endif
#if defined(USE_DMR)
	float dmrTXLevel             = m_conf.getModemDMRTXLevel();
#else
	float dmrTXLevel             = 0.0F;
#endif
#if defined(USE_YSF)
	float ysfTXLevel             = m_conf.getModemYSFTXLevel();
#else
	float ysfTXLevel             = 0.0F;
#endif
#if defined(USE_P25)
	float p25TXLevel             = m_conf.getModemP25TXLevel();
#else
	float p25TXLevel             = 0.0F;
#endif
#if defined(USE_NXDN)
	float nxdnTXLevel            = m_conf.getModemNXDNTXLevel();
#else
	float nxdnTXLevel            = 0.0F;
#endif
#if defined(USE_POCSAG)
	float pocsagTXLevel          = m_conf.getModemPOCSAGTXLevel();
#else
	float pocsagTXLevel          = 0.0F;
#endif
#if defined(USE_FM)
	float fmTXLevel              = m_conf.getModemFMTXLevel();
#else
	float fmTXLevel              = 0.0F;
#endif
	bool trace                   = m_conf.getModemTrace();
	bool debug                   = m_conf.getModemDebug();
#if defined(USE_DMR)
	unsigned int colorCode       = m_conf.getDMRColorCode();
	bool trunking                = m_conf.getDMRTrunkingEnabled();
	bool controlChannel          = m_conf.getDMRControlChannel();
#endif
#if defined(USE_YSF)
	bool lowDeviation            = m_conf.getFusionLowDeviation();
	unsigned int ysfTXHang       = m_conf.getFusionTXHang();
#endif
#if defined(USE_P25)
	unsigned int p25TXHang       = m_conf.getP25TXHang();
#endif
#if defined(USE_NXDN)
	unsigned int nxdnTXHang      = m_conf.getNXDNTXHang();
#endif
	unsigned int rxFrequency     = m_conf.getModemRXFrequency();
	unsigned int txFrequency     = m_conf.getModemTXFrequency();
#if defined(USE_POCSAG)
	unsigned int pocsagFrequency = m_conf.getPOCSAGFrequency();
#else
	unsigned int pocsagFrequency = txFrequency;
#endif
	int rxOffset                 = m_conf.getModemRXOffset();
	int txOffset                 = m_conf.getModemTXOffset();
	int rxDCOffset               = m_conf.getModemRXDCOffset();
	int txDCOffset               = m_conf.getModemTXDCOffset();
	float rfLevel                = m_conf.getModemRFLevel();
	bool useCOSAsLockout         = m_conf.getModemUseCOSAsLockout();

	LogInfo("Modem Parameters");
	LogInfo("    Protocol: %s", protocol.c_str());

	if (protocol == "uart") {
		LogInfo("    UART Port: %s", uartPort.c_str());
		LogInfo("    UART Speed: %u", uartSpeed);
	} else if (protocol == "udp") {
		LogInfo("    Modem Address: %s", modemAddress.c_str());
		LogInfo("    Modem Port: %hu", modemPort);
		LogInfo("    Local Address: %s", localAddress.c_str());
		LogInfo("    Local Port: %hu", localPort);
	}
#if defined(__linux__)
	else if (protocol == "i2c") {
		LogInfo("    I2C Port: %s", i2cPort.c_str());
		LogInfo("    I2C Address: %02X", i2cAddress);
	}
#endif

	LogInfo("    RX Invert: %s", rxInvert ? "yes" : "no");
	LogInfo("    TX Invert: %s", txInvert ? "yes" : "no");
	LogInfo("    PTT Invert: %s", pttInvert ? "yes" : "no");
	LogInfo("    TX Delay: %ums", txDelay);
	LogInfo("    RX Offset: %dHz", rxOffset);
	LogInfo("    TX Offset: %dHz", txOffset);
	LogInfo("    RX DC Offset: %d", rxDCOffset);
	LogInfo("    TX DC Offset: %d", txDCOffset);
	LogInfo("    RF Level: %.1f%%", rfLevel);
#if defined(USE_DMR)
	LogInfo("    DMR Delay: %u (%.1fms)", dmrDelay, float(dmrDelay) * 0.0416666F);
#endif
	LogInfo("    RX Level: %.1f%%", rxLevel);
	LogInfo("    CW Id TX Level: %.1f%%", cwIdTXLevel);
#if defined(USE_DSTAR)
	LogInfo("    D-Star TX Level: %.1f%%", dstarTXLevel);
#endif
#if defined(USE_DMR)
	LogInfo("    DMR TX Level: %.1f%%", dmrTXLevel);
#endif
#if defined(USE_YSF)
	LogInfo("    YSF TX Level: %.1f%%", ysfTXLevel);
#endif
#if defined(USE_P25)
	LogInfo("    P25 TX Level: %.1f%%", p25TXLevel);
#endif
#if defined(USE_NXDN)
	LogInfo("    NXDN TX Level: %.1f%%", nxdnTXLevel);
#endif
#if defined(USE_POCSAG)
	LogInfo("    POCSAG TX Level: %.1f%%", pocsagTXLevel);
#endif
#if defined(USE_FM)
	LogInfo("    FM TX Level: %.1f%%", fmTXLevel);
#endif
	LogInfo("    TX Frequency: %uHz (%uHz)", txFrequency, txFrequency + txOffset);
	LogInfo("    RX Frequency: %uHz (%uHz)", rxFrequency, rxFrequency + rxOffset);
	LogInfo("    Use COS as Lockout: %s", useCOSAsLockout ? "yes" : "no");

	m_modem = new CModem(m_duplex, rxInvert, txInvert, pttInvert, txDelay, dmrDelay, useCOSAsLockout, trace, debug);

	IModemPort* port = nullptr;
	if (protocol == "uart")
		port = new CUARTController(uartPort, uartSpeed, true);
	else if (protocol == "udp")
		port = new CUDPController(modemAddress, modemPort, localAddress, localPort);
#if defined(__linux__)
	else if (protocol == "i2c")
		port = new CI2CController(i2cPort, i2cAddress);
#endif
	else if (protocol == "null")
		port = new CNullController;
	else
		return false;

	m_modem->setPort(port);
	m_modem->setModeParams(m_dstarEnabled, m_dmrEnabled, m_ysfEnabled, m_p25Enabled, m_nxdnEnabled, m_pocsagEnabled, m_fmEnabled);
	m_modem->setLevels(rxLevel, cwIdTXLevel, dstarTXLevel, dmrTXLevel, ysfTXLevel, p25TXLevel, nxdnTXLevel, pocsagTXLevel, fmTXLevel);
	m_modem->setRFParams(rxFrequency, rxOffset, txFrequency, txOffset, txDCOffset, rxDCOffset, rfLevel, pocsagFrequency);
#if defined(USE_DMR)
	m_modem->setDMRParams(colorCode);
	if (trunking)
		m_modem->setDMRTrunkingParams(controlChannel);
#endif
#if defined(USE_YSF)
	m_modem->setYSFParams(lowDeviation, ysfTXHang);
#endif
#if defined(USE_P25)
	m_modem->setP25Params(p25TXHang);
#endif
#if defined(USE_NXDN)
	m_modem->setNXDNParams(nxdnTXHang);
#endif

#if defined(USE_FM)
	if (m_fmEnabled) {
		std::string  callsign             = m_conf.getFMCallsign();
		unsigned int callsignSpeed        = m_conf.getFMCallsignSpeed();
		unsigned int callsignFrequency    = m_conf.getFMCallsignFrequency();
		unsigned int callsignTime         = m_conf.getFMCallsignTime();
		unsigned int callsignHoldoff      = m_conf.getFMCallsignHoldoff();
		float        callsignHighLevel    = m_conf.getFMCallsignHighLevel();
		float        callsignLowLevel     = m_conf.getFMCallsignLowLevel();
		bool         callsignAtStart      = m_conf.getFMCallsignAtStart();
		bool         callsignAtEnd        = m_conf.getFMCallsignAtEnd();
		bool         callsignAtLatch      = m_conf.getFMCallsignAtLatch();
		std::string  rfAck                = m_conf.getFMRFAck();
		unsigned int ackSpeed             = m_conf.getFMAckSpeed();
		unsigned int ackFrequency         = m_conf.getFMAckFrequency();
		unsigned int ackMinTime           = m_conf.getFMAckMinTime();
		unsigned int ackDelay             = m_conf.getFMAckDelay();
		float        ackLevel             = m_conf.getFMAckLevel();
		unsigned int timeout              = m_conf.getFMTimeout();
		float        timeoutLevel         = m_conf.getFMTimeoutLevel();
		float        ctcssFrequency       = m_conf.getFMCTCSSFrequency();
		unsigned int ctcssHighThreshold   = m_conf.getFMCTCSSHighThreshold();
		unsigned int ctcssLowThreshold    = m_conf.getFMCTCSSLowThreshold();
		float        ctcssLevel           = m_conf.getFMCTCSSLevel();
		unsigned int kerchunkTime         = m_conf.getFMKerchunkTime();
		unsigned int hangTime             = m_conf.getFMHangTime();
		unsigned int accessMode           = m_conf.getFMAccessMode();
		bool         linkMode             = m_conf.getFMLinkMode();
		bool         cosInvert            = m_conf.getFMCOSInvert();
		bool         noiseSquelch         = m_conf.getFMNoiseSquelch();
		unsigned int squelchHighThreshold = m_conf.getFMSquelchHighThreshold();
		unsigned int squelchLowThreshold  = m_conf.getFMSquelchLowThreshold();
		unsigned int rfAudioBoost         = m_conf.getFMRFAudioBoost();
		float        maxDevLevel          = m_conf.getFMMaxDevLevel();
		unsigned int modeHangTime         = m_conf.getFMModeHang();

		LogInfo("FM Parameters");
		LogInfo("    Callsign: %s", callsign.c_str());
		LogInfo("    Callsign Speed: %uWPM", callsignSpeed);
		LogInfo("    Callsign Frequency: %uHz", callsignFrequency);
		LogInfo("    Callsign Time: %umins", callsignTime);
		LogInfo("    Callsign Holdoff: 1/%u", callsignHoldoff);
		LogInfo("    Callsign High Level: %.1f%%", callsignHighLevel);
		LogInfo("    Callsign Low Level: %.1f%%", callsignLowLevel);
		LogInfo("    Callsign At Start: %s", callsignAtStart ? "yes" : "no");
		LogInfo("    Callsign At End: %s", callsignAtEnd ? "yes" : "no");
		LogInfo("    Callsign At Latch: %s", callsignAtLatch ? "yes" : "no");
		LogInfo("    RF Ack: %s", rfAck.c_str());
		LogInfo("    Ack Speed: %uWPM", ackSpeed);
		LogInfo("    Ack Frequency: %uHz", ackFrequency);
		LogInfo("    Ack Min Time: %us", ackMinTime);
		LogInfo("    Ack Delay: %ums", ackDelay);
		LogInfo("    Ack Level: %.1f%%", ackLevel);
		LogInfo("    Timeout: %us", timeout);
		LogInfo("    Timeout Level: %.1f%%", timeoutLevel);
		LogInfo("    CTCSS Frequency: %.1fHz", ctcssFrequency);
		LogInfo("    CTCSS High Threshold: %u", ctcssHighThreshold);
		LogInfo("    CTCSS Low Threshold: %u", ctcssLowThreshold);
		LogInfo("    CTCSS Level: %.1f%%", ctcssLevel);
		LogInfo("    Kerchunk Time: %us", kerchunkTime);
		LogInfo("    Hang Time: %us", hangTime);
		LogInfo("    Access Mode: %u", accessMode);
		LogInfo("    Link Mode: %s", linkMode ? "yes" : "no");
		LogInfo("    COS Invert: %s", cosInvert ? "yes" : "no");

		LogInfo("    Noise Squelch: %s", noiseSquelch ? "yes" : "no");
		if (noiseSquelch) {
			LogInfo("    Squelch High Threshold: %u", squelchHighThreshold);
			LogInfo("    Squelch Low Threshold: %u", squelchLowThreshold);
		}

		LogInfo("    RF Audio Boost: x%u", rfAudioBoost);
		LogInfo("    Max. Deviation Level: %.1f%%", maxDevLevel);
		LogInfo("    Mode Hang: %us", modeHangTime);

		m_modem->setFMCallsignParams(callsign, callsignSpeed, callsignFrequency, callsignTime, callsignHoldoff, callsignHighLevel, callsignLowLevel, callsignAtStart, callsignAtEnd, callsignAtLatch);
		m_modem->setFMAckParams(rfAck, ackSpeed, ackFrequency, ackMinTime, ackDelay, ackLevel);
		m_modem->setFMMiscParams(timeout, timeoutLevel, ctcssFrequency, ctcssHighThreshold, ctcssLowThreshold, ctcssLevel, kerchunkTime, hangTime, accessMode, linkMode, cosInvert, noiseSquelch, squelchHighThreshold, squelchLowThreshold, rfAudioBoost, maxDevLevel);

		if (m_conf.getFMNetworkEnabled()) {
			std::string  extAck        = m_conf.getFMExtAck();
			unsigned int extAudioBoost = m_conf.getFMExtAudioBoost();

			LogInfo("    Ext. Ack: %s", extAck.c_str());
			LogInfo("    Ext. Audio Boost: x%u", extAudioBoost);

			m_modem->setFMExtParams(extAck, extAudioBoost);
		}
	}
#endif
	bool ret = m_modem->open();
	if (!ret) {
		delete m_modem;
		m_modem = nullptr;
		return false;
	}

	return true;
}

#if defined(USE_DSTAR)
bool CMMDVMHost::createDStarNetwork()
{
	std::string gatewayAddress = m_conf.getDStarGatewayAddress();
	unsigned short gatewayPort = m_conf.getDStarGatewayPort();
	std::string localAddress   = m_conf.getDStarLocalAddress();
	unsigned short localPort   = m_conf.getDStarLocalPort();
	bool debug                 = m_conf.getDStarNetworkDebug();
	m_dstarNetModeHang         = m_conf.getDStarNetworkModeHang();

	LogInfo("D-Star Network Parameters");
	LogInfo("    Gateway Address: %s", gatewayAddress.c_str());
	LogInfo("    Gateway Port: %hu", gatewayPort);
	LogInfo("    Local Address: %s", localAddress.c_str());
	LogInfo("    Local Port: %hu", localPort);
	LogInfo("    Mode Hang: %us", m_dstarNetModeHang);

	m_dstarNetwork = new CDStarNetwork(gatewayAddress, gatewayPort, localAddress, localPort, m_duplex, VERSION, debug);

	bool ret = m_dstarNetwork->open();
	if (!ret) {
		delete m_dstarNetwork;
		m_dstarNetwork = nullptr;
		return false;
	}

	m_dstarNetwork->enable(!m_listenOnly);

	return true;
}
#endif

#if defined(USE_DMR)
bool CMMDVMHost::createDMRNetwork()
{
	std::string gatewayAddress = m_conf.getDMRNetworkGatewayAddress();
	unsigned short gatewayPort = m_conf.getDMRNetworkGatewayPort();
	std::string localAddress   = m_conf.getDMRNetworkLocalAddress();
	unsigned short localPort   = m_conf.getDMRNetworkLocalPort();
	unsigned int id      = m_conf.getDMRId();
	bool debug           = m_conf.getDMRNetworkDebug();
	unsigned int jitter  = m_conf.getDMRNetworkJitter();
	bool slot1           = m_conf.getDMRNetworkSlot1();
	bool slot2           = m_conf.getDMRNetworkSlot2();
	m_dmrNetModeHang     = m_conf.getDMRNetworkModeHang();

	LogInfo("DMR Network Parameters");
	LogInfo("    Gateway Address: %s", gatewayAddress.c_str());
	LogInfo("    Gateway Port: %hu", gatewayPort);
	LogInfo("    Local Address: %s", localAddress.c_str());
	LogInfo("    Local Port: %hu", localPort);
	LogInfo("    Jitter: %ums", jitter);
	LogInfo("    Slot 1: %s", slot1 ? "enabled" : "disabled");
	LogInfo("    Slot 2: %s", slot2 ? "enabled" : "disabled");
	LogInfo("    Mode Hang: %us", m_dmrNetModeHang);

	m_dmrNetwork = new CDMRNetwork(gatewayAddress, gatewayPort, localAddress, localPort, id, m_duplex, slot1, slot2, debug);
	
	bool ret = m_dmrNetwork->open();
	if (!ret) {
		delete m_dmrNetwork;
		m_dmrNetwork = nullptr;
		return false;
	}

	m_dmrNetwork->enable(!m_listenOnly);

	return true;
}
#endif

#if defined(USE_YSF)
bool CMMDVMHost::createYSFNetwork()
{
	std::string localAddress   = m_conf.getFusionNetworkLocalAddress();
	unsigned short localPort   = m_conf.getFusionNetworkLocalPort();
	std::string gatewayAddress = m_conf.getFusionNetworkGatewayAddress();
	unsigned short gatewayPort = m_conf.getFusionNetworkGatewayPort();
	m_ysfNetModeHang           = m_conf.getFusionNetworkModeHang();
	bool debug                 = m_conf.getFusionNetworkDebug();

	LogInfo("System Fusion Network Parameters");
	LogInfo("    Local Address: %s", localAddress.c_str());
	LogInfo("    Local Port: %hu", localPort);
	LogInfo("    Gateway Address: %s", gatewayAddress.c_str());
	LogInfo("    Gateway Port: %hu", gatewayPort);
	LogInfo("    Mode Hang: %us", m_ysfNetModeHang);

	m_ysfNetwork = new CYSFNetwork(localAddress, localPort, gatewayAddress, gatewayPort, m_callsign, debug);

	bool ret = m_ysfNetwork->open();
	if (!ret) {
		delete m_ysfNetwork;
		m_ysfNetwork = nullptr;
		return false;
	}

	m_ysfNetwork->enable(!m_listenOnly);

	return true;
}
#endif

#if defined(USE_P25)
bool CMMDVMHost::createP25Network()
{
	std::string gatewayAddress = m_conf.getP25GatewayAddress();
	unsigned short gatewayPort = m_conf.getP25GatewayPort();
	std::string localAddress   = m_conf.getP25LocalAddress();
	unsigned short localPort   = m_conf.getP25LocalPort();
	m_p25NetModeHang           = m_conf.getP25NetworkModeHang();
	bool debug                 = m_conf.getP25NetworkDebug();

	LogInfo("P25 Network Parameters");
	LogInfo("    Gateway Address: %s", gatewayAddress.c_str());
	LogInfo("    Gateway Port: %hu", gatewayPort);
	LogInfo("    Local Address: %s", localAddress.c_str());
	LogInfo("    Local Port: %hu", localPort);
	LogInfo("    Mode Hang: %us", m_p25NetModeHang);

	m_p25Network = new CP25Network(gatewayAddress, gatewayPort, localAddress, localPort, debug);

	bool ret = m_p25Network->open();
	if (!ret) {
		delete m_p25Network;
		m_p25Network = nullptr;
		return false;
	}

	m_p25Network->enable(!m_listenOnly);

	return true;
}
#endif

#if defined(USE_NXDN)
bool CMMDVMHost::createNXDNNetwork()
{
	std::string protocol       = m_conf.getNXDNNetworkProtocol();
	std::string gatewayAddress = m_conf.getNXDNGatewayAddress();
	unsigned short gatewayPort = m_conf.getNXDNGatewayPort();
	std::string localAddress   = m_conf.getNXDNLocalAddress();
	unsigned short localPort   = m_conf.getNXDNLocalPort();
	m_nxdnNetModeHang          = m_conf.getNXDNNetworkModeHang();
	bool debug                 = m_conf.getNXDNNetworkDebug();

	LogInfo("NXDN Network Parameters");
	LogInfo("    Protocol: %s", protocol.c_str());
	LogInfo("    Gateway Address: %s", gatewayAddress.c_str());
	LogInfo("    Gateway Port: %hu", gatewayPort);
	LogInfo("    Local Address: %s", localAddress.c_str());
	LogInfo("    Local Port: %hu", localPort);
	LogInfo("    Mode Hang: %us", m_nxdnNetModeHang);

	if (protocol == "Kenwood")
		m_nxdnNetwork = new CNXDNKenwoodNetwork(localAddress, localPort, gatewayAddress, gatewayPort, debug);
	else
		m_nxdnNetwork = new CNXDNIcomNetwork(localAddress, localPort, gatewayAddress, gatewayPort, debug);

	bool ret = m_nxdnNetwork->open();
	if (!ret) {
		delete m_nxdnNetwork;
		m_nxdnNetwork = nullptr;
		return false;
	}

	m_nxdnNetwork->enable(!m_listenOnly);

	return true;
}
#endif

#if defined(USE_POCSAG)
bool CMMDVMHost::createPOCSAGNetwork()
{
	std::string gatewayAddress = m_conf.getPOCSAGGatewayAddress();
	unsigned short gatewayPort = m_conf.getPOCSAGGatewayPort();
	std::string localAddress   = m_conf.getPOCSAGLocalAddress();
	unsigned short localPort   = m_conf.getPOCSAGLocalPort();
	m_pocsagNetModeHang        = m_conf.getPOCSAGNetworkModeHang();
	bool debug                 = m_conf.getPOCSAGNetworkDebug();

	LogInfo("POCSAG Network Parameters");
	LogInfo("    Gateway Address: %s", gatewayAddress.c_str());
	LogInfo("    Gateway Port: %hu", gatewayPort);
	LogInfo("    Local Address: %s", localAddress.c_str());
	LogInfo("    Local Port: %hu", localPort);
	LogInfo("    Mode Hang: %us", m_pocsagNetModeHang);

	m_pocsagNetwork = new CPOCSAGNetwork(localAddress, localPort, gatewayAddress, gatewayPort, debug);

	bool ret = m_pocsagNetwork->open();
	if (!ret) {
		delete m_pocsagNetwork;
		m_pocsagNetwork = nullptr;
		return false;
	}

	m_pocsagNetwork->enable(!m_listenOnly);

	return true;
}
#endif

#if defined(USE_FM)
bool CMMDVMHost::createFMNetwork()
{
	std::string callsign       = m_conf.getFMCallsign();
	std::string gatewayAddress = m_conf.getFMGatewayAddress();
	unsigned short gatewayPort = m_conf.getFMGatewayPort();
	std::string localAddress   = m_conf.getFMLocalAddress();
	unsigned short localPort   = m_conf.getFMLocalPort();
	bool preEmphasis           = m_conf.getFMPreEmphasis();
	bool deEmphasis            = m_conf.getFMDeEmphasis();
	float txAudioGain          = m_conf.getFMTXAudioGain();
	float rxAudioGain          = m_conf.getFMRXAudioGain();
	m_fmNetModeHang            = m_conf.getFMNetworkModeHang();
	bool debug                 = m_conf.getFMNetworkDebug();

	LogInfo("FM Network Parameters");
	LogInfo("    Gateway Address: %s", gatewayAddress.c_str());
	LogInfo("    Gateway Port: %hu", gatewayPort);
	LogInfo("    Local Address: %s", localAddress.c_str());
	LogInfo("    Local Port: %hu", localPort);
	LogInfo("    Pre-Emphasis: %s", preEmphasis ? "yes" : "no");
	LogInfo("    De-Emphasis: %s", deEmphasis ? "yes" : "no");
	LogInfo("    TX Audio Gain: %.2f", txAudioGain);
	LogInfo("    RX Audio Gain: %.2f", rxAudioGain);
	LogInfo("    Mode Hang: %us", m_fmNetModeHang);

	m_fmNetwork = new CFMNetwork(callsign, localAddress, localPort, gatewayAddress, gatewayPort, debug);

	bool ret = m_fmNetwork->open();
	if (!ret) {
		delete m_fmNetwork;
		m_fmNetwork = nullptr;
		return false;
	}

	m_fmNetwork->enable(!m_listenOnly);

	return true;
}
#endif

void CMMDVMHost::readParams()
{
#if defined(USE_DSTAR)
	m_dstarEnabled  = m_conf.getDStarEnabled();
#endif
#if defined(USE_DMR)
	m_dmrEnabled    = m_conf.getDMREnabled();
#endif
#if defined(USE_YSF)
	m_ysfEnabled    = m_conf.getFusionEnabled();
#endif
#if defined(USE_P25)
	m_p25Enabled    = m_conf.getP25Enabled();
#endif
#if defined(USE_NXDN)
	m_nxdnEnabled   = m_conf.getNXDNEnabled();
#endif
#if defined(USE_POCSAG)
	m_pocsagEnabled = m_conf.getPOCSAGEnabled();
#endif
#if defined(USE_FM)
	m_fmEnabled     = m_conf.getFMEnabled();
#endif
	m_duplex        = m_conf.getDuplex();
	m_callsign      = m_conf.getCallsign();
	m_id            = m_conf.getId();
	m_timeout       = m_conf.getTimeout();

	LogInfo("General Parameters");
	LogInfo("    Callsign: %s", m_callsign.c_str());
#if defined(USE_DMR) || defined(USE_P25)
	LogInfo("    Id: %u", m_id);
#endif
	LogInfo("    Duplex: %s", m_duplex ? "yes" : "no");
	LogInfo("    Timeout: %us", m_timeout);
#if defined(USE_DSTAR)
	LogInfo("    D-Star: %s", m_dstarEnabled ? "enabled" : "disabled");
#endif
#if defined(USE_DMR)
	LogInfo("    DMR: %s", m_dmrEnabled ? "enabled" : "disabled");
#endif
#if defined(USE_YSF)
	LogInfo("    YSF: %s", m_ysfEnabled ? "enabled" : "disabled");
#endif
#if defined(USE_P25)
	LogInfo("    P25: %s", m_p25Enabled ? "enabled" : "disabled");
#endif
#if defined(USE_NXDN)
	LogInfo("    NXDN: %s", m_nxdnEnabled ? "enabled" : "disabled");
#endif
#if defined(USE_POCSAG)
	LogInfo("    POCSAG: %s", m_pocsagEnabled ? "enabled" : "disabled");
#endif
#if defined(USE_FM)
	LogInfo("    FM: %s", m_fmEnabled ? "enabled" : "disabled");
#endif
}

void CMMDVMHost::setMode(unsigned char mode)
{
	assert(m_modem != nullptr);

	switch (mode) {
#if defined(USE_DSTAR)
	case MODE_DSTAR:
		if (m_dstarNetwork != nullptr && m_dstarEnabled)
			m_dstarNetwork->enable(!m_listenOnly);
#if defined(USE_DMR)
		if (m_dmrNetwork != nullptr)
			m_dmrNetwork->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysfNetwork != nullptr)
			m_ysfNetwork->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25Network != nullptr)
			m_p25Network->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdnNetwork != nullptr)
			m_nxdnNetwork->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsagNetwork != nullptr)
			m_pocsagNetwork->enable(false);
#endif
#if defined(USE_FM)
		if (m_fmNetwork != nullptr)
			m_fmNetwork->enable(false);
#endif
		if (m_dstar != nullptr && m_dstarEnabled)
			m_dstar->enable(true);
#if defined(USE_DMR)
		if (m_dmr != nullptr)
			m_dmr->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysf != nullptr)
			m_ysf->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25 != nullptr)
			m_p25->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdn != nullptr)
			m_nxdn->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsag != nullptr)
			m_pocsag->enable(false);
#endif
#if defined(USE_FM)
		if (m_fm != nullptr)
			m_fm->enable(false);
#endif
		if (m_dstarEnabled) {
			m_modem->setMode(MODE_DSTAR);
			m_mode = MODE_DSTAR;
			m_modeTimer.start();
			m_cwIdTimer.stop();
			createLockFile("D-Star");
			writeJSONMode("D-Star");
		}
		break;
#endif
#if defined(USE_DMR)
	case MODE_DMR:
#if defined(USE_DSTAR)
		if (m_dstarNetwork != nullptr)
			m_dstarNetwork->enable(false);
#endif
		if (m_dmrNetwork != nullptr && m_dmrEnabled)
			m_dmrNetwork->enable(!m_listenOnly);
#if defined(USE_YSF)
		if (m_ysfNetwork != nullptr)
			m_ysfNetwork->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25Network != nullptr)
			m_p25Network->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdnNetwork != nullptr)
			m_nxdnNetwork->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsagNetwork != nullptr)
			m_pocsagNetwork->enable(false);
#endif
#if defined(USE_FM)
		if (m_fmNetwork != nullptr)
			m_fmNetwork->enable(false);
#endif
#if defined(USE_DSTAR)
		if (m_dstar != nullptr)
			m_dstar->enable(false);
#endif
		if (m_dmr != nullptr && m_dmrEnabled)
			m_dmr->enable(true);
#if defined(USE_YSF)
		if (m_ysf != nullptr)
			m_ysf->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25 != nullptr)
			m_p25->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdn != nullptr)
			m_nxdn->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsag != nullptr)
			m_pocsag->enable(false);
#endif
#if defined(USE_FM)
		if (m_fm != nullptr)
			m_fm->enable(false);
#endif
		if (m_dmrEnabled) {
			m_modem->setMode(MODE_DMR);
			if (m_duplex) {
				m_modem->writeDMRStart(true);
				m_dmrTXTimer.start();
			}
			m_mode = MODE_DMR;
			m_modeTimer.start();
			m_cwIdTimer.stop();
			createLockFile("DMR");
			writeJSONMode("DMR");
		}
		break;
#endif
#if defined(USE_YSF)
	case MODE_YSF:
#if defined(USE_DSTAR)
		if (m_dstarNetwork != nullptr)
			m_dstarNetwork->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmrNetwork != nullptr)
			m_dmrNetwork->enable(false);
#endif
		if (m_ysfNetwork != nullptr && m_ysfEnabled)
			m_ysfNetwork->enable(!m_listenOnly);
#if defined(USE_P25)
		if (m_p25Network != nullptr)
			m_p25Network->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdnNetwork != nullptr)
			m_nxdnNetwork->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsagNetwork != nullptr)
			m_pocsagNetwork->enable(false);
#endif
#if defined(USE_FM)
		if (m_fmNetwork != nullptr)
			m_fmNetwork->enable(false);
#endif
#if defined(USE_DSTAR)
		if (m_dstar != nullptr)
			m_dstar->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmr != nullptr)
			m_dmr->enable(false);
#endif
		if (m_ysf != nullptr && m_ysfEnabled)
			m_ysf->enable(true);
#if defined(USE_P25)
		if (m_p25 != nullptr)
			m_p25->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdn != nullptr)
			m_nxdn->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsag != nullptr)
			m_pocsag->enable(false);
#endif
#if defined(USE_FM)
		if (m_fm != nullptr)
			m_fm->enable(false);
#endif
		if (m_ysfEnabled) {
			m_modem->setMode(MODE_YSF);
			m_mode = MODE_YSF;
			m_modeTimer.start();
			m_cwIdTimer.stop();
			createLockFile("System Fusion");
			writeJSONMode("YSF");
		}
		break;
#endif
#if defined(USE_P25)
	case MODE_P25:
#if defined(USE_DSTAR)
		if (m_dstarNetwork != nullptr)
			m_dstarNetwork->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmrNetwork != nullptr)
			m_dmrNetwork->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysfNetwork != nullptr)
			m_ysfNetwork->enable(false);
#endif
		if (m_p25Network != nullptr && m_p25Enabled)
			m_p25Network->enable(!m_listenOnly);
#if defined(USE_NXDN)
		if (m_nxdnNetwork != nullptr)
			m_nxdnNetwork->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsagNetwork != nullptr)
			m_pocsagNetwork->enable(false);
#endif
#if defined(USE_FM)
		if (m_fmNetwork != nullptr)
			m_fmNetwork->enable(false);
#endif
#if defined(USE_DSTAR)
		if (m_dstar != nullptr)
			m_dstar->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmr != nullptr)
			m_dmr->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysf != nullptr)
			m_ysf->enable(false);
#endif
		if (m_p25 != nullptr && m_p25Enabled)
			m_p25->enable(true);
#if defined(USE_NXDN)
		if (m_nxdn != nullptr)
			m_nxdn->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsag != nullptr)
			m_pocsag->enable(false);
#endif
#if defined(USE_FM)
		if (m_fm != nullptr)
			m_fm->enable(false);
#endif
		if (m_p25Enabled) {
			m_modem->setMode(MODE_P25);
			m_mode = MODE_P25;
			m_modeTimer.start();
			m_cwIdTimer.stop();
			createLockFile("P25");
			writeJSONMode("P25");
		}
		break;
#endif
#if defined(USE_NXDN)
	case MODE_NXDN:
#if defined(USE_DSTAR)
		if (m_dstarNetwork != nullptr)
			m_dstarNetwork->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmrNetwork != nullptr)
			m_dmrNetwork->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysfNetwork != nullptr)
			m_ysfNetwork->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25Network != nullptr)
			m_p25Network->enable(false);
#endif
		if (m_nxdnNetwork != nullptr && m_nxdnEnabled)
			m_nxdnNetwork->enable(!m_listenOnly);
#if defined(USE_POCSAG)
		if (m_pocsagNetwork != nullptr)
			m_pocsagNetwork->enable(false);
#endif
#if defined(USE_FM)
		if (m_fmNetwork != nullptr)
			m_fmNetwork->enable(false);
#endif
#if defined(USE_DSTAR)
		if (m_dstar != nullptr)
			m_dstar->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmr != nullptr)
			m_dmr->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysf != nullptr)
			m_ysf->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25 != nullptr)
			m_p25->enable(false);
#endif
		if (m_nxdn != nullptr && m_nxdnEnabled)
			m_nxdn->enable(true);
#if defined(USE_POCSAG)
		if (m_pocsag != nullptr)
			m_pocsag->enable(false);
#endif
#if defined(USE_FM)
		if (m_fm != nullptr)
			m_fm->enable(false);
#endif
		if (m_nxdnEnabled) {
			m_modem->setMode(MODE_NXDN);
			m_mode = MODE_NXDN;
			m_modeTimer.start();
			m_cwIdTimer.stop();
			createLockFile("NXDN");
			writeJSONMode("NXDN");
		}
		break;
#endif
#if defined(USE_POCSAG)
	case MODE_POCSAG:
#if defined(USE_DSTAR)
		if (m_dstarNetwork != nullptr)
			m_dstarNetwork->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmrNetwork != nullptr)
			m_dmrNetwork->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysfNetwork != nullptr)
			m_ysfNetwork->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25Network != nullptr)
			m_p25Network->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdnNetwork != nullptr)
			m_nxdnNetwork->enable(false);
#endif
		if (m_pocsagNetwork != nullptr)
			m_pocsagNetwork->enable(!m_listenOnly);
#if defined(USE_FM)
		if (m_fmNetwork != nullptr)
			m_fmNetwork->enable(false);
#endif
#if defined(USE_DSTAR)
		if (m_dstar != nullptr)
			m_dstar->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmr != nullptr)
			m_dmr->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysf != nullptr)
			m_ysf->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25 != nullptr)
			m_p25->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdn != nullptr)
			m_nxdn->enable(false);
#endif
		if (m_pocsag != nullptr)
			m_pocsag->enable(true);
#if defined(USE_FM)
		if (m_fm != nullptr)
			m_fm->enable(false);
#endif
		m_modem->setMode(MODE_POCSAG);
		m_mode = MODE_POCSAG;
		m_modeTimer.start();
		m_cwIdTimer.stop();
		createLockFile("POCSAG");
		writeJSONMode("POCSAG");
		break;
#endif
#if defined(USE_FM)
	case MODE_FM:
#if defined(USE_DSTAR)
		if (m_dstarNetwork != nullptr)
			m_dstarNetwork->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmrNetwork != nullptr)
			m_dmrNetwork->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysfNetwork != nullptr)
			m_ysfNetwork->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25Network != nullptr)
			m_p25Network->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdnNetwork != nullptr)
			m_nxdnNetwork->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsagNetwork != nullptr)
			m_pocsagNetwork->enable(false);
#endif
		if (m_fmNetwork != nullptr && m_fmEnabled)
			m_fmNetwork->enable(!m_listenOnly);
#if defined(USE_DSTAR)
		if (m_dstar != nullptr)
			m_dstar->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmr != nullptr)
			m_dmr->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysf != nullptr)
			m_ysf->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25 != nullptr)
			m_p25->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdn != nullptr)
			m_nxdn->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsag != nullptr)
			m_pocsag->enable(false);
		if (m_fm != nullptr && m_fmEnabled)
			m_fm->enable(true);
#endif
#if defined(USE_DMR)
		if (m_mode == MODE_DMR && m_duplex && m_modem->hasTX()) {
			m_modem->writeDMRStart(false);
			m_dmrTXTimer.stop();
		}
#endif
		if (m_fmEnabled) {
			m_modem->setMode(MODE_FM);
			m_mode = MODE_FM;
			m_modeTimer.start();
			m_cwIdTimer.stop();
			createLockFile("FM");
			writeJSONMode("FM");
		}
		break;
#endif

	case MODE_LOCKOUT:
#if defined(USE_DSTAR)
		if (m_dstarNetwork != nullptr)
			m_dstarNetwork->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmrNetwork != nullptr)
			m_dmrNetwork->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysfNetwork != nullptr)
			m_ysfNetwork->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25Network != nullptr)
			m_p25Network->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdnNetwork != nullptr)
			m_nxdnNetwork->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsagNetwork != nullptr)
			m_pocsagNetwork->enable(false);
#endif
#if defined(USE_FM)
		if (m_fmNetwork != nullptr)
			m_fmNetwork->enable(false);
#endif
#if defined(USE_DSTAR)
		if (m_dstar != nullptr)
			m_dstar->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmr != nullptr)
			m_dmr->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysf != nullptr)
			m_ysf->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25 != nullptr)
			m_p25->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdn != nullptr)
			m_nxdn->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsag != nullptr)
			m_pocsag->enable(false);
#endif
#if defined(USE_FM)
		if (m_fm != nullptr)
			m_fm->enable(false);
#endif
#if defined(USE_DMR)
		if (m_mode == MODE_DMR && m_duplex && m_modem->hasTX()) {
			m_modem->writeDMRStart(false);
			m_dmrTXTimer.stop();
		}
#endif
		m_modem->setMode(MODE_IDLE);
		m_mode = MODE_LOCKOUT;
		m_modeTimer.stop();
		m_cwIdTimer.stop();
		removeLockFile();
		writeJSONMode("lockout");
		break;

	case MODE_ERROR:
		LogMessage("Mode set to Error");
#if defined(USE_DSTAR)
		if (m_dstarNetwork != nullptr)
			m_dstarNetwork->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmrNetwork != nullptr)
			m_dmrNetwork->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysfNetwork != nullptr)
			m_ysfNetwork->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25Network != nullptr)
			m_p25Network->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdnNetwork != nullptr)
			m_nxdnNetwork->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsagNetwork != nullptr)
			m_pocsagNetwork->enable(false);
#endif
#if defined(USE_FM)
		if (m_fmNetwork != nullptr)
			m_fmNetwork->enable(false);
#endif
#if defined(USE_DSTAR)
		if (m_dstar != nullptr)
			m_dstar->enable(false);
#endif
#if defined(USE_DMR)
		if (m_dmr != nullptr)
			m_dmr->enable(false);
#endif
#if defined(USE_YSF)
		if (m_ysf != nullptr)
			m_ysf->enable(false);
#endif
#if defined(USE_P25)
		if (m_p25 != nullptr)
			m_p25->enable(false);
#endif
#if defined(USE_NXDN)
		if (m_nxdn != nullptr)
			m_nxdn->enable(false);
#endif
#if defined(USE_POCSAG)
		if (m_pocsag != nullptr)
			m_pocsag->enable(false);
#endif
#if defined(USE_FM)
		if (m_fm != nullptr)
			m_fm->enable(false);
#endif
#if defined(USE_DMR)
		if (m_mode == MODE_DMR && m_duplex && m_modem->hasTX()) {
			m_modem->writeDMRStart(false);
			m_dmrTXTimer.stop();
		}
#endif
		m_mode = MODE_ERROR;
		m_modeTimer.stop();
		m_cwIdTimer.stop();
		removeLockFile();
		writeJSONMode("error");
		break;

	default:
#if defined(USE_DSTAR)
		if (m_dstarNetwork != nullptr && m_dstarEnabled)
			m_dstarNetwork->enable(!m_listenOnly);
#endif
#if defined(USE_DMR)
		if (m_dmrNetwork != nullptr && m_dmrEnabled)
			m_dmrNetwork->enable(!m_listenOnly);
#endif
#if defined(USE_YSF)
		if (m_ysfNetwork != nullptr && m_ysfEnabled)
			m_ysfNetwork->enable(!m_listenOnly);
#endif
#if defined(USE_P25)
		if (m_p25Network != nullptr && m_p25Enabled)
			m_p25Network->enable(!m_listenOnly);
#endif
#if defined(USE_NXDN)
		if (m_nxdnNetwork != nullptr && m_nxdnEnabled)
			m_nxdnNetwork->enable(!m_listenOnly);
#endif
#if defined(USE_POCSAG)
		if (m_pocsagNetwork != nullptr)
			m_pocsagNetwork->enable(!m_listenOnly);
#endif
#if defined(USE_FM)
		if (m_fmNetwork != nullptr && m_fmEnabled)
			m_fmNetwork->enable(!m_listenOnly);
#endif
#if defined(USE_DSTAR)
		if (m_dstar != nullptr && m_dstarEnabled)
			m_dstar->enable(true);
#endif
#if defined(USE_DMR)
		if (m_dmr != nullptr && m_dmrEnabled)
			m_dmr->enable(true);
#endif
#if defined(USE_YSF)
		if (m_ysf != nullptr && m_ysfEnabled)
			m_ysf->enable(true);
#endif
#if defined(USE_P25)
		if (m_p25 != nullptr && m_p25Enabled)
			m_p25->enable(true);
#endif
#if defined(USE_NXDN)
		if (m_nxdn != nullptr && m_nxdnEnabled)
			m_nxdn->enable(true);
#endif
#if defined(USE_POCSAG)
		if (m_pocsag != nullptr)
			m_pocsag->enable(true);
#endif
#if defined(USE_FM)
		if (m_fm != nullptr && m_fmEnabled)
			m_fm->enable(true);
#endif
#if defined(USE_DMR)
		if (m_mode == MODE_DMR && m_duplex && m_modem->hasTX() && m_dmrEnabled) {
			m_modem->writeDMRStart(false);
			m_dmrTXTimer.stop();
		}
#endif
		m_modem->setMode(MODE_IDLE);
		if (m_mode == MODE_ERROR) {
			m_modem->sendCWId(m_callsign);
			m_cwIdTimer.setTimeout(m_cwIdTime);
			m_cwIdTimer.start();
		} else {
			m_cwIdTimer.setTimeout(m_cwIdTime / 4U);
			m_cwIdTimer.start();
		}
		m_mode = MODE_IDLE;
		m_modeTimer.stop();
		removeLockFile();
		writeJSONMode("idle");
		break;
	}
}

void CMMDVMHost::createLockFile(const char* mode) const
{
	if (m_lockFileEnabled) {
		FILE* fp = ::fopen(m_lockFileName.c_str(), "wt");
		if (fp != nullptr) {
			::fprintf(fp, "%s\n", mode);
			::fclose(fp);
		}
	}
}

void CMMDVMHost::removeLockFile() const
{
	if (m_lockFileEnabled)
		::remove(m_lockFileName.c_str());
}

// Listen-only: RF is still heard (and published), but nothing is
// transmitted and every network is off, until turned off again.
bool g_listenOnly = false;

void CMMDVMHost::setListenOnly(bool on)
{
	if (on == m_listenOnly)
		return;

	m_listenOnly = on;
	g_listenOnly = on;
	m_modem->setTXInhibit(on);

#if defined(USE_DSTAR)
	if (m_dstarNetwork != nullptr)
		m_dstarNetwork->enable(!on && m_dstarEnabled);
#endif
#if defined(USE_DMR)
	if (m_dmrNetwork != nullptr)
		m_dmrNetwork->enable(!on && m_dmrEnabled);
#endif
#if defined(USE_YSF)
	if (m_ysfNetwork != nullptr)
		m_ysfNetwork->enable(!on && m_ysfEnabled);
#endif
#if defined(USE_P25)
	if (m_p25Network != nullptr)
		m_p25Network->enable(!on && m_p25Enabled);
#endif
#if defined(USE_NXDN)
	if (m_nxdnNetwork != nullptr)
		m_nxdnNetwork->enable(!on && m_nxdnEnabled);
#endif
#if defined(USE_POCSAG)
	if (m_pocsagNetwork != nullptr)
		m_pocsagNetwork->enable(!on && m_pocsagEnabled);
#endif
#if defined(USE_FM)
	if (m_fmNetwork != nullptr)
		m_fmNetwork->enable(!on && m_fmEnabled);
#endif

	if (on)
		m_cwIdTimer.stop();
	else if (m_cwIdTimer.getTimeout() > 0U)
		m_cwIdTimer.start();

	LogMessage("Listen-only %s", on ? "on: hearing, not transmitting, networks off" : "off");
}

void CMMDVMHost::remoteControl(const std::string& commandString)
{
	if (m_remoteControl == nullptr)
		return;

	REMOTE_COMMAND command = m_remoteControl->getCommand(commandString);
	switch (command) {
		case REMOTE_COMMAND::MODE_IDLE:
			m_fixedMode = false;
			setMode(MODE_IDLE);
			break;
		case REMOTE_COMMAND::MODE_LOCKOUT:
			m_fixedMode = false;
			setMode(MODE_LOCKOUT);
			break;
#if defined(USE_DSTAR)
		case REMOTE_COMMAND::MODE_DSTAR:
			if (m_dstar != nullptr)
				processModeCommand(MODE_DSTAR, m_dstarRFModeHang);
			break;
#endif
#if defined(USE_DMR)
		case REMOTE_COMMAND::MODE_DMR:
			if (m_dmr != nullptr)
				processModeCommand(MODE_DMR, m_dmrRFModeHang);
			break;
#endif
#if defined(USE_YSF)
		case REMOTE_COMMAND::MODE_YSF:
			if (m_ysf != nullptr)
				processModeCommand(MODE_YSF, m_ysfRFModeHang);
			break;
#endif
#if defined(USE_P25)
		case REMOTE_COMMAND::MODE_P25:
			if (m_p25 != nullptr)
				processModeCommand(MODE_P25, m_p25RFModeHang);
			break;
#endif
#if defined(USE_NXDN)
		case REMOTE_COMMAND::MODE_NXDN:
			if (m_nxdn != nullptr)
				processModeCommand(MODE_NXDN, m_nxdnRFModeHang);
			break;
#endif
#if defined(USE_FM)
		case REMOTE_COMMAND::MODE_FM:
			if (m_fmEnabled)
				processModeCommand(MODE_FM, 0);
			break;
#endif
#if defined(USE_DSTAR)
		case REMOTE_COMMAND::ENABLE_DSTAR:
			if (m_dstar != nullptr && !m_dstarEnabled)
				processEnableCommand(m_dstarEnabled, true);
			if (m_dstarNetwork != nullptr)
				m_dstarNetwork->enable(!m_listenOnly);
			break;
#endif
#if defined(USE_DMR)
		case REMOTE_COMMAND::ENABLE_DMR:
			if (m_dmr != nullptr && !m_dmrEnabled)
				processEnableCommand(m_dmrEnabled, true);
			if (m_dmrNetwork != nullptr)
				m_dmrNetwork->enable(!m_listenOnly);
			break;
#endif
#if defined(USE_YSF)
		case REMOTE_COMMAND::ENABLE_YSF:
			if (m_ysf != nullptr && !m_ysfEnabled)
				processEnableCommand(m_ysfEnabled, true);
			if (m_ysfNetwork != nullptr)
				m_ysfNetwork->enable(!m_listenOnly);
			break;
#endif
#if defined(USE_P25)
		case REMOTE_COMMAND::ENABLE_P25:
			if (m_p25 != nullptr && !m_p25Enabled)
				processEnableCommand(m_p25Enabled, true);
			if (m_p25Network != nullptr)
				m_p25Network->enable(!m_listenOnly);
			break;
#endif
#if defined(USE_NXDN)
		case REMOTE_COMMAND::ENABLE_NXDN:
			if (m_nxdn != nullptr && !m_nxdnEnabled)
				processEnableCommand(m_nxdnEnabled, true);
			if (m_nxdnNetwork != nullptr)
				m_nxdnNetwork->enable(!m_listenOnly);
			break;
#endif
#if defined(USE_FM)
		case REMOTE_COMMAND::ENABLE_FM:
			if (!m_fmEnabled)
				processEnableCommand(m_fmEnabled, true);
			break;
#endif
#if defined(USE_DSTAR)
		case REMOTE_COMMAND::DISABLE_DSTAR:
			if (m_dstar != nullptr && m_dstarEnabled)
				processEnableCommand(m_dstarEnabled, false);
			if (m_dstarNetwork != nullptr)
				m_dstarNetwork->enable(false);
			break;
#endif
#if defined(USE_DMR)
		case REMOTE_COMMAND::DISABLE_DMR:
			if (m_dmr != nullptr && m_dmrEnabled)
				processEnableCommand(m_dmrEnabled, false);
			if (m_dmrNetwork != nullptr)
				m_dmrNetwork->enable(false);
			break;
#endif
#if defined(USE_YSF)
		case REMOTE_COMMAND::DISABLE_YSF:
			if (m_ysf != nullptr && m_ysfEnabled)
				processEnableCommand(m_ysfEnabled, false);
			if (m_ysfNetwork != nullptr)
				m_ysfNetwork->enable(false);
			break;
#endif
#if defined(USE_P25)
		case REMOTE_COMMAND::DISABLE_P25:
			if (m_p25 != nullptr && m_p25Enabled)
				processEnableCommand(m_p25Enabled, false);
			if (m_p25Network != nullptr)
				m_p25Network->enable(false);
			break;
#endif
#if defined(USE_NXDN)
		case REMOTE_COMMAND::DISABLE_NXDN:
			if (m_nxdn != nullptr && m_nxdnEnabled)
				processEnableCommand(m_nxdnEnabled, false);
			if (m_nxdnNetwork != nullptr)
				m_nxdnNetwork->enable(false);
			break;
#endif
#if defined(USE_FM)
		case REMOTE_COMMAND::DISABLE_FM:
			if (m_fmEnabled)
				processEnableCommand(m_fmEnabled, false);
			break;
#endif
#if defined(USE_POCSAG)
		case REMOTE_COMMAND::PAGE:
			if (m_pocsag != nullptr) {
				unsigned int ric = m_remoteControl->getArgUInt(0U);
				std::string text;
				for (unsigned int i = 1U; i < m_remoteControl->getArgCount(); i++) {
					if (i > 1U)
						text += " ";
					text += m_remoteControl->getArgString(i);
				}
				m_pocsag->sendPage(ric, text);
			}
			break;
		case REMOTE_COMMAND::PAGE_BCD:
			if (m_pocsag != nullptr) {
				unsigned int ric = m_remoteControl->getArgUInt(0U);
				std::string text;
				for (unsigned int i = 1U; i < m_remoteControl->getArgCount(); i++) {
					if (i > 1U)
						text += " ";
					text += m_remoteControl->getArgString(i);
				}
				m_pocsag->sendPageBCD(ric, text);
			}
			break;
		case REMOTE_COMMAND::PAGE_A1:
			if (m_pocsag != nullptr) {
				unsigned int ric = m_remoteControl->getArgUInt(0U);
				m_pocsag->sendPageAlert1(ric);
			}
			break;
		case REMOTE_COMMAND::PAGE_A2:
			if (m_pocsag != nullptr) {
				unsigned int ric = m_remoteControl->getArgUInt(0U);
				std::string text;
				for (unsigned int i = 1U; i < m_remoteControl->getArgCount(); i++) {
					if (i > 1U)
						text += " ";
					text += m_remoteControl->getArgString(i);
				}
				m_pocsag->sendPageAlert2(ric, text);
			}
			break;
#endif
		case REMOTE_COMMAND::CW:
			setMode(MODE_IDLE); // Force the modem to go idle so that we can send the CW text.
			if (!m_modem->hasTX()) {
				std::string cwtext;
				for (unsigned int i = 0U; i < m_remoteControl->getArgCount(); i++) {
					if (i > 0U)
						cwtext += " ";
					cwtext += m_remoteControl->getArgString(i);
				}
				writeJSONMessage("Sending CW ID");
				m_modem->sendCWId(cwtext);
			}
			break;
		case REMOTE_COMMAND::RELOAD:
			m_reload = true;
			break;
		case REMOTE_COMMAND::LISTEN_ON:
			setListenOnly(true);
			break;
		case REMOTE_COMMAND::LISTEN_OFF:
			setListenOnly(false);
			break;
		default:
			break;
	}
}

void CMMDVMHost::processModeCommand(unsigned char mode, unsigned int timeout)
{
	m_fixedMode = false;
	m_modeTimer.setTimeout(timeout);

	if (m_remoteControl->getArgCount() > 0U) {
		if (m_remoteControl->getArgString(0U) == "fixed") {
			m_fixedMode = true;
		} else {
			unsigned int t = m_remoteControl->getArgUInt(0U);
			if (t > 0U)
				m_modeTimer.setTimeout(t);
		}
	}

	setMode(mode);
}

void CMMDVMHost::processEnableCommand(bool& mode, bool enabled)
{
	LogDebug("Setting mode current=%s new=%s", mode ? "true" : "false", enabled ? "true" : "false");

	mode = enabled;

	m_modem->setModeParams(m_dstarEnabled, m_dmrEnabled, m_ysfEnabled, m_p25Enabled, m_nxdnEnabled, m_pocsagEnabled, m_fmEnabled);
	if (!m_modem->writeConfig())
		LogError("Cannot write Config to MMDVM");
}

void CMMDVMHost::buildNetworkStatusString(std::string &str)
{
	str = "";

#if defined(USE_DSTAR)
	str += std::string("dstar:") + (((m_dstarNetwork == nullptr) || !m_dstarEnabled) ? "n/a" : (m_dstarNetwork->isConnected() ? "conn" : "disc"));
#endif
#if defined(USE_DMR)
	str += std::string(" dmr:") + (((m_dmrNetwork == nullptr) || !m_dmrEnabled) ? "n/a" : (m_dmrNetwork->isConnected() ? "conn" : "disc"));
#endif
#if defined(USE_YSF)
	str += std::string(" ysf:") + (((m_ysfNetwork == nullptr) || !m_ysfEnabled) ? "n/a" : (m_ysfNetwork->isConnected() ? "conn" : "disc"));
#endif
#if defined(USE_P25)
	str += std::string(" p25:") + (((m_p25Network == nullptr) || !m_p25Enabled) ? "n/a" : (m_p25Network->isConnected() ? "conn" : "disc"));
#endif
#if defined(USE_NXDN)
	str += std::string(" nxdn:") + (((m_nxdnNetwork == nullptr) || !m_nxdnEnabled) ? "n/a" : (m_nxdnNetwork->isConnected() ? "conn" : "disc"));
#endif
#if defined(USE_FM)
	str += std::string(" fm:") + (m_fmEnabled ? "conn" : "n/a");
#endif
}

void CMMDVMHost::buildNetworkHostsString(std::string &str)
{
	str = "";

#if defined(USE_DSTAR)
	std::string dstarReflector;
	if (m_dstarEnabled && (m_dstarNetwork != nullptr)) {
		unsigned char ref[DSTAR_LONG_CALLSIGN_LENGTH + 1];
		LINK_STATUS status;

		::memset(ref, 0, sizeof(ref));

		m_dstarNetwork->getStatus(status, &ref[0]);
		switch (status) {
			case LINK_STATUS::LINKED_LOOPBACK:
			case LINK_STATUS::LINKED_DEXTRA:
			case LINK_STATUS::LINKED_DPLUS:
			case LINK_STATUS::LINKED_DCS:
			case LINK_STATUS::LINKED_CCS:
			     dstarReflector = std::string((char *)ref);
			     break;

			default:
			     break;
		}
	}

	str += std::string("dstar:\"") + ((dstarReflector.length() == 0) ? "NONE" : dstarReflector) + "\"";
#endif
#if defined(USE_DMR)
	str += std::string(" dmr:\"") + ((m_dmrEnabled && (m_dmrNetwork != nullptr)) ? m_conf.getDMRNetworkGatewayAddress() : "NONE") + "\"";
#endif
#if defined(USE_YSF)
	str += std::string(" ysf:\"") + ((m_ysfEnabled && (m_ysfNetwork != nullptr)) ? m_conf.getFusionNetworkGatewayAddress() : "NONE") + "\"";
#endif
#if defined(USE_P25)
	str += std::string(" p25:\"") + ((m_p25Enabled && (m_p25Network != nullptr)) ? m_conf.getP25GatewayAddress() : "NONE") + "\"";
#endif
#if defined(USE_NXDN)
	str += std::string(" nxdn:\"") + ((m_nxdnEnabled && (m_nxdnNetwork != nullptr)) ? m_conf.getNXDNGatewayAddress() : "NONE") + "\"";
#endif
#if defined(USE_FM)
	str += std::string(" fm:\"") + ((m_fmEnabled && (m_fmNetwork != nullptr)) ? m_conf.getFMGatewayAddress() : "NONE") + "\"";
#endif
}

void CMMDVMHost::writeJSONMode(const std::string& mode)
{
	nlohmann::json json;

	json["timestamp"] = CUtils::createTimestamp();
	json["mode"]      = mode;

	WriteJSON("MMDVM", json);
}

void CMMDVMHost::writeJSONMessage(const std::string& message)
{
	nlohmann::json json;

	json["timestamp"] = CUtils::createTimestamp();
	json["message"]   = message;

	WriteJSON("MMDVM", json);
}

void CMMDVMHost::writeSerial(const unsigned char* message, unsigned int length)
{
	assert(m_modem != nullptr);
	assert(message != nullptr);

	if (length > 5000U)
		return;

	if (length <= 252U) {
		// Simple case of a short message, send it immediately to the modem
		m_modem->writeSerialData(message, length);
	} else {
		::memcpy(m_serialBuffer, message, length);
		m_serialLength = length;

		m_modem->writeSerialData(m_serialBuffer, 252U);
		m_serialStart = 252U;

		m_serialTimer.start();
	}
}

void CMMDVMHost::onCommand(const unsigned char* command, unsigned int length)
{
	assert(host != nullptr);
	assert(command != nullptr);

	host->remoteControl(std::string((char*)command, length));
}

void CMMDVMHost::onDisplay(const unsigned char* message, unsigned int length)
{
	assert(host != nullptr);
	assert(message != nullptr);

	host->writeSerial(message, length);
}
