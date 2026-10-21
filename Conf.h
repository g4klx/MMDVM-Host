/*
 *   Copyright (C) 2015-2026 by Jonathan Naylor G4KLX
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

#if !defined(CONF_H)
#define	CONF_H

#include <string>
#include <vector>

#include <cstdint>

class CConf
{
public:
	CConf(const std::string& file);
	~CConf();

	bool read();

	// The General section
	std::string  getCallsign() const;
	unsigned int getId() const;
	unsigned int getTimeout() const;
	bool         getDuplex() const;
	bool         getDaemon() const;

	// The Log section
	unsigned int getLogMQTTLevel() const;
	unsigned int getLogDisplayLevel() const;

	// The MQTT section
	std::string    getMQTTHost() const;
	unsigned short getMQTTPort() const;
	unsigned int   getMQTTKeepalive() const;
	std::string    getMQTTName() const;
	bool           getMQTTAuthEnabled() const;
	std::string    getMQTTUsername() const;
	std::string    getMQTTPassword() const;

	// The CW ID section
	bool         getCWIdEnabled() const;
	unsigned int getCWIdTime() const;
	std::string  getCWIdCallsign() const;

#if defined(USE_DMR) || defined(USE_P25)
	// The DMR Id section
	std::string  getDMRIdLookupFile() const;
	unsigned int getDMRIdLookupTime() const;
#endif

#if defined(USE_NXDN)
	// The NXDN Id section
	std::string  getNXDNIdLookupFile() const;
	unsigned int getNXDNIdLookupTime() const;
#endif

	// The Modem section
	std::string  getModemProtocol() const;
	std::string  getModemUARTPort() const;
	unsigned int getModemUARTSpeed() const;
	std::string  getModemI2CPort() const;
	unsigned int getModemI2CAddress() const;
	std::string  getModemModemAddress() const;
	unsigned short getModemModemPort() const;
	std::string  getModemLocalAddress() const;
	unsigned short getModemLocalPort() const;
	unsigned int getModemRXFrequency() const;
	unsigned int getModemTXFrequency() const;
	bool         getModemRXInvert() const;
	bool         getModemTXInvert() const;
	bool         getModemPTTInvert() const;
	unsigned int getModemTXDelay() const;
#if defined(USE_DMR)
	unsigned int getModemDMRDelay() const;
#endif
	int          getModemTXOffset() const;
	int          getModemRXOffset() const;
	int          getModemRXDCOffset() const;
	int          getModemTXDCOffset() const;
	float        getModemRFLevel() const;
	float        getModemRXLevel() const;
	float        getModemCWIdTXLevel() const;
#if defined(USE_DSTAR)
	float        getModemDStarTXLevel() const;
#endif
#if defined(USE_DMR)
	float        getModemDMRTXLevel() const;
#endif
#if defined(USE_YSF)
	float        getModemYSFTXLevel() const;
#endif
#if defined(USE_P25)
	float        getModemP25TXLevel() const;
#endif
#if defined(USE_NXDN)
	float        getModemNXDNTXLevel() const;
#endif
#if defined(USE_POCSAG)
	float        getModemPOCSAGTXLevel() const;
#endif
#if defined(USE_FM)
	float        getModemFMTXLevel() const;
#endif
	std::string  getModemRSSIMappingFile() const;
	bool         getModemUseCOSAsLockout() const;
	bool         getModemTrace() const;
	bool         getModemDebug() const;

	// The Transparent Data section
	bool         getTransparentEnabled() const;
	std::string  getTransparentRemoteAddress() const;
	unsigned short getTransparentRemotePort() const;
	unsigned short getTransparentLocalPort() const;
	unsigned int getTransparentSendFrameType() const;

#if defined(USE_DSTAR)
	// The D-Star section
	bool         getDStarEnabled() const;
	std::string  getDStarModule() const;
	bool         getDStarSelfOnly() const;
	std::vector<std::string> getDStarBlackList() const;
	std::vector<std::string> getDStarWhiteList() const;
	bool         getDStarAckReply() const;
	unsigned int getDStarAckTime() const;
	DSTAR_ACK    getDStarAckMessage() const;
	bool         getDStarErrorReply() const;
	bool         getDStarRemoteGateway() const;
	unsigned int getDStarModeHang() const;
#endif

#if defined(USE_DMR)
	// The DMR section
	bool         getDMREnabled() const;
	DMR_BEACONS  getDMRBeacons() const;
	unsigned int getDMRBeaconInterval() const;
	unsigned int getDMRBeaconDuration() const;
	unsigned int getDMRId() const;
	unsigned int getDMRColorCode() const;
	bool         getDMREmbeddedLCOnly() const;
	bool         getDMRDumpTAData() const;
	bool         getDMRSelfOnly() const;
	std::vector<unsigned int> getDMRPrefixes() const;
	std::vector<unsigned int> getDMRBlackList() const;
	std::vector<unsigned int> getDMRWhiteList() const;
	std::vector<unsigned int> getDMRSlot1TGWhiteList() const;
	std::vector<unsigned int> getDMRSlot2TGWhiteList() const;
	unsigned int getDMRCallHang() const;
	unsigned int getDMRDuplexControlTG() const;
	unsigned int getDMRTXHang() const;
	unsigned int getDMRModeHang() const;
	DMR_OVCM     getDMROVCM() const;
	bool         getDMRProtect() const;
	bool         getDMRTrunkingEnabled() const;
	bool         getDMRControlChannel() const;
	bool         getDMRControlChannelAlternateSlot() const;
	unsigned int getDMRSystemCode() const;
	bool         getDMRRegistrationRequired() const;
#endif

#if defined(USE_YSF)
	// The System Fusion section
	bool          getFusionEnabled() const;
	bool          getFusionLowDeviation() const;
	bool          getFusionRemoteGateway() const;
	bool          getFusionSelfOnly() const;
	unsigned int  getFusionTXHang() const;
	unsigned int  getFusionModeHang() const;
#endif

#if defined(USE_P25)
	// The P25 section
	bool         getP25Enabled() const;
	unsigned int getP25Id() const;
	unsigned int getP25NAC() const;
	bool         getP25SelfOnly() const;
	bool         getP25OverrideUID() const;
	bool         getP25RemoteGateway() const;
	bool         getP25SoftId() const;
	unsigned int getP25TXHang() const;
	unsigned int getP25ModeHang() const;
#endif

#if defined(USE_NXDN)
	// The NXDN section
	bool         getNXDNEnabled() const;
	unsigned int getNXDNId() const;
	unsigned int getNXDNRAN() const;
	bool         getNXDNSelfOnly() const;
	bool         getNXDNRemoteGateway() const;
	unsigned int getNXDNTXHang() const;
	unsigned int getNXDNModeHang() const;
#endif

#if defined(USE_POCSAG)
	// The POCSAG section
	bool         getPOCSAGEnabled() const;
	unsigned int getPOCSAGFrequency() const;
#endif

#if defined(USE_FM)
	// The FM Section
	bool         getFMEnabled() const;
	std::string  getFMCallsign() const;
	unsigned int getFMCallsignSpeed() const;
	unsigned int getFMCallsignFrequency() const;
	unsigned int getFMCallsignTime() const;
	unsigned int getFMCallsignHoldoff() const;
	float        getFMCallsignHighLevel() const;
	float        getFMCallsignLowLevel() const;
	bool         getFMCallsignAtStart() const;
	bool         getFMCallsignAtEnd() const;
	bool         getFMCallsignAtLatch() const;
	std::string  getFMRFAck() const;
	std::string  getFMExtAck() const;
	unsigned int getFMAckSpeed() const;
	unsigned int getFMAckFrequency() const;
	unsigned int getFMAckMinTime() const;
	unsigned int getFMAckDelay() const;
	float        getFMAckLevel() const;
	unsigned int getFMTimeout() const;
	float        getFMTimeoutLevel() const;
	float        getFMCTCSSFrequency() const;
	unsigned int getFMCTCSSHighThreshold() const;
	unsigned int getFMCTCSSLowThreshold() const;
	float        getFMCTCSSLevel() const;
	unsigned int getFMKerchunkTime() const;
	unsigned int getFMHangTime() const;
	unsigned int getFMAccessMode() const;
	bool         getFMLinkMode() const;
	bool         getFMCOSInvert() const;
	bool         getFMNoiseSquelch() const;
	unsigned int getFMSquelchHighThreshold() const;
	unsigned int getFMSquelchLowThreshold() const;
	unsigned int getFMRFAudioBoost() const;
	float        getFMMaxDevLevel() const;
	unsigned int getFMExtAudioBoost() const;
	unsigned int getFMModeHang() const;
#endif

#if defined(USE_DSTAR)
	// The D-Star Network section
	bool         getDStarNetworkEnabled() const;
	std::string  getDStarGatewayAddress() const;
	unsigned short getDStarGatewayPort() const;
	std::string  getDStarLocalAddress() const;
	unsigned short getDStarLocalPort() const;
	unsigned int getDStarNetworkModeHang() const;
	bool         getDStarNetworkDebug() const;
#endif

#if defined(USE_DMR)
	// The DMR Network section
	bool         getDMRNetworkEnabled() const;
	std::string  getDMRNetworkGatewayAddress() const;
	unsigned short getDMRNetworkGatewayPort() const;
	std::string  getDMRNetworkLocalAddress() const;
	unsigned short getDMRNetworkLocalPort() const;
	bool         getDMRNetworkDebug() const;
	unsigned int getDMRNetworkJitter() const;
	bool         getDMRNetworkSlot1() const;
	bool         getDMRNetworkSlot2() const;
	unsigned int getDMRNetworkModeHang() const;
#endif

#if defined(USE_YSF)
	// The System Fusion Network section
	bool         getFusionNetworkEnabled() const;
	std::string  getFusionNetworkLocalAddress() const;
	unsigned short getFusionNetworkLocalPort() const;
	std::string  getFusionNetworkGatewayAddress() const;
	unsigned short getFusionNetworkGatewayPort() const;
	unsigned int getFusionNetworkModeHang() const;
	bool         getFusionNetworkDebug() const;
#endif

#if defined(USE_P25)
	// The P25 Network section
	bool         getP25NetworkEnabled() const;
	std::string  getP25GatewayAddress() const;
	unsigned short getP25GatewayPort() const;
	std::string  getP25LocalAddress() const;
	unsigned short getP25LocalPort() const;
	unsigned int getP25NetworkModeHang() const;
	bool         getP25NetworkDebug() const;
#endif

#if defined(USE_NXDN)
	// The NXDN Network section
	bool         getNXDNNetworkEnabled() const;
	std::string  getNXDNNetworkProtocol() const;
	std::string  getNXDNGatewayAddress() const;
	unsigned short getNXDNGatewayPort() const;
	std::string  getNXDNLocalAddress() const;
	unsigned short getNXDNLocalPort() const;
	unsigned int getNXDNNetworkModeHang() const;
	bool         getNXDNNetworkDebug() const;
#endif

#if defined(USE_POCSAG)
	// The POCSAG Network section
	bool         getPOCSAGNetworkEnabled() const;
	std::string  getPOCSAGGatewayAddress() const;
	unsigned short getPOCSAGGatewayPort() const;
	std::string  getPOCSAGLocalAddress() const;
	unsigned short getPOCSAGLocalPort() const;
	unsigned int getPOCSAGNetworkModeHang() const;
	bool         getPOCSAGNetworkDebug() const;
#endif

#if defined(USE_FM)
	// The FM Network section
	bool         getFMNetworkEnabled() const;
	std::string  getFMGatewayAddress() const;
	unsigned short getFMGatewayPort() const;
	std::string  getFMLocalAddress() const;
	unsigned short getFMLocalPort() const;
	bool         getFMPreEmphasis() const;
	bool         getFMDeEmphasis() const;
	float        getFMTXAudioGain() const;
	float        getFMRXAudioGain() const;
	unsigned int getFMNetworkModeHang() const;
	bool         getFMNetworkDebug() const;
#endif

	// The Lock File section
	bool         getLockFileEnabled() const;
	std::string  getLockFileName() const;

	// The Remote Control section
	bool         getRemoteControlEnabled() const;

private:
	std::string  m_file;
	std::string  m_callsign;
	unsigned int m_id;
	unsigned int m_timeout;
	bool         m_duplex;
	bool         m_daemon;

	unsigned int m_logMQTTLevel;
	unsigned int m_logDisplayLevel;

	std::string  m_mqttHost;
	unsigned short m_mqttPort;
	unsigned int m_mqttKeepalive;
	std::string  m_mqttName;
	bool         m_mqttAuthEnabled;
	std::string  m_mqttUsername;
	std::string  m_mqttPassword;

	bool         m_cwIdEnabled;
	unsigned int m_cwIdTime;
	std::string  m_cwIdCallsign;

#if defined(USE_DMR) || defined(USE_P25)
	std::string  m_dmrIdLookupFile;
	unsigned int m_dmrIdLookupTime;
#endif

#if defined(USE_NXDN)
	std::string  m_nxdnIdLookupFile;
	unsigned int m_nxdnIdLookupTime;
#endif

	std::string  m_modemProtocol;
	std::string  m_modemUARTPort;
	unsigned int m_modemUARTSpeed;
	std::string  m_modemI2CPort;
	unsigned int m_modemI2CAddress;
	std::string  m_modemModemAddress;
	unsigned short m_modemModemPort;
	std::string  m_modemLocalAddress;
	unsigned short m_modemLocalPort;
	unsigned int m_modemRXFrequency;
	unsigned int m_modemTXFrequency;
	bool         m_modemRXInvert;
	bool         m_modemTXInvert;
	bool         m_modemPTTInvert;
	unsigned int m_modemTXDelay;
#if defined(USE_DMR)
	unsigned int m_modemDMRDelay;
#endif
	int          m_modemTXOffset;
	int          m_modemRXOffset;
	int          m_modemRXDCOffset;
	int          m_modemTXDCOffset;
	float        m_modemRFLevel;
	float        m_modemRXLevel;
	float        m_modemCWIdTXLevel;
	float        m_modemDStarTXLevel;
	float        m_modemDMRTXLevel;
	float        m_modemYSFTXLevel;
	float        m_modemP25TXLevel;
	float        m_modemNXDNTXLevel;
	float        m_modemPOCSAGTXLevel;
	float        m_modemFMTXLevel;
	std::string  m_modemRSSIMappingFile;
	bool         m_modemUseCOSAsLockout;
	bool         m_modemTrace;
	bool         m_modemDebug;

	bool         m_transparentEnabled;
	std::string  m_transparentRemoteAddress;
	unsigned short m_transparentRemotePort;
	unsigned short m_transparentLocalPort;
	unsigned int m_transparentSendFrameType;

#if defined(USE_DSTAR)
	bool         m_dstarEnabled;
	std::string  m_dstarModule;
	bool         m_dstarSelfOnly;
	std::vector<std::string> m_dstarBlackList;
	std::vector<std::string> m_dstarWhiteList;
	bool         m_dstarAckReply;
	unsigned int m_dstarAckTime;
	DSTAR_ACK    m_dstarAckMessage;
	bool         m_dstarErrorReply;
	bool         m_dstarRemoteGateway;
#endif
	unsigned int m_dstarModeHang;

#if defined(USE_DMR)
	bool         m_dmrEnabled;
	DMR_BEACONS  m_dmrBeacons;
	unsigned int m_dmrBeaconInterval;
	unsigned int m_dmrBeaconDuration;
#endif
	unsigned int m_dmrId;
#if defined(USE_DMR)
	unsigned int m_dmrColorCode;
	bool         m_dmrSelfOnly;
	bool         m_dmrEmbeddedLCOnly;
	bool         m_dmrDumpTAData;
	std::vector<unsigned int> m_dmrPrefixes;
	std::vector<unsigned int> m_dmrBlackList;
	std::vector<unsigned int> m_dmrWhiteList;
	std::vector<unsigned int> m_dmrSlot1TGWhiteList;
	std::vector<unsigned int> m_dmrSlot2TGWhiteList;
	unsigned int m_dmrCallHang;
	unsigned int m_dmrDuplexControlTG;
	unsigned int m_dmrTXHang;
#endif
	unsigned int m_dmrModeHang;
#if defined(USE_DMR)
	DMR_OVCM     m_dmrOVCM;
	bool         m_dmrProtect;
	bool         m_dmrTrunkingEnabled;
	bool         m_dmrControlChannel;
	bool         m_dmrControlChannelAlternateSlot;
	unsigned int m_dmrSystemCode;
	bool         m_dmrRegistrationRequired;
#endif

#if defined(USE_YSF)
	bool          m_fusionEnabled;
	bool          m_fusionLowDeviation;
	bool          m_fusionRemoteGateway;
	bool          m_fusionSelfOnly;
	unsigned int  m_fusionTXHang;
#endif
	unsigned int  m_fusionModeHang;

#if defined(USE_P25)
	bool         m_p25Enabled;
#endif
	unsigned int m_p25Id;
#if defined(USE_P25)
	unsigned int m_p25NAC;
	bool         m_p25SelfOnly;
	bool         m_p25OverrideUID;
	bool         m_p25RemoteGateway;
	bool         m_p25SoftId;
	unsigned int m_p25TXHang;
#endif
	unsigned int m_p25ModeHang;

#if defined(USE_NXDN)
	bool         m_nxdnEnabled;
	unsigned int m_nxdnId;
	unsigned int m_nxdnRAN;
	bool         m_nxdnSelfOnly;
	bool         m_nxdnRemoteGateway;
	unsigned int m_nxdnTXHang;
#endif
	unsigned int m_nxdnModeHang;

#if defined(USE_POCSAG)
	bool         m_pocsagEnabled;
#endif
	unsigned int m_pocsagFrequency;

#if defined(USE_FM)
	bool         m_fmEnabled;
#endif
	std::string  m_fmCallsign;
#if defined(USE_FM)
	unsigned int m_fmCallsignSpeed;
	unsigned int m_fmCallsignFrequency;
	unsigned int m_fmCallsignTime;
	unsigned int m_fmCallsignHoldoff;
	float        m_fmCallsignHighLevel;
	float        m_fmCallsignLowLevel;
	bool         m_fmCallsignAtStart;
	bool         m_fmCallsignAtEnd;
	bool         m_fmCallsignAtLatch;
	std::string  m_fmRFAck;
	std::string  m_fmExtAck;
	unsigned int m_fmAckSpeed;
	unsigned int m_fmAckFrequency;
	unsigned int m_fmAckMinTime;
	unsigned int m_fmAckDelay;
	float        m_fmAckLevel;
#endif
	unsigned int m_fmTimeout;
#if defined(USE_FM)
	float        m_fmTimeoutLevel;
	float        m_fmCTCSSFrequency;
	unsigned int m_fmCTCSSHighThreshold;
	unsigned int m_fmCTCSSLowThreshold;
	float        m_fmCTCSSLevel;
	unsigned int m_fmKerchunkTime;
	unsigned int m_fmHangTime;
	unsigned int m_fmAccessMode;
	bool         m_fmLinkMode;
	bool         m_fmCOSInvert;
	bool         m_fmNoiseSquelch;
	unsigned int m_fmSquelchHighThreshold;
	unsigned int m_fmSquelchLowThreshold;
	unsigned int m_fmRFAudioBoost;
	float        m_fmMaxDevLevel;
	unsigned int m_fmExtAudioBoost;
#endif
	unsigned int m_fmModeHang;

#if defined(USE_DSTAR)
	bool         m_dstarNetworkEnabled;
	std::string  m_dstarGatewayAddress;
	unsigned short m_dstarGatewayPort;
	std::string  m_dstarLocalAddress;
	unsigned short m_dstarLocalPort;
#endif
	unsigned int m_dstarNetworkModeHang;
#if defined(USE_DSTAR)
	bool         m_dstarNetworkDebug;
#endif

#if defined(USE_DMR)
	bool         m_dmrNetworkEnabled;
	std::string  m_dmrNetworkGatewayAddress;
	unsigned short m_dmrNetworkGatewayPort;
	std::string  m_dmrNetworkLocalAddress;
	unsigned short m_dmrNetworkLocalPort;
	bool         m_dmrNetworkDebug;
	unsigned int m_dmrNetworkJitter;
	bool         m_dmrNetworkSlot1;
	bool         m_dmrNetworkSlot2;
#endif
	unsigned int m_dmrNetworkModeHang;

#if defined(USE_YSF)
	bool         m_fusionNetworkEnabled;
	std::string  m_fusionNetworkLocalAddress;
	unsigned short m_fusionNetworkLocalPort;
	std::string  m_fusionNetworkGatewayAddress;
	unsigned short m_fusionNetworkGatewayPort;
#endif
	unsigned int m_fusionNetworkModeHang;
#if defined(USE_YSF)
	bool         m_fusionNetworkDebug;
#endif

#if defined(USE_P25)
	bool         m_p25NetworkEnabled;
	std::string  m_p25GatewayAddress;
	unsigned short m_p25GatewayPort;
	std::string  m_p25LocalAddress;
	unsigned short m_p25LocalPort;
#endif
	unsigned int m_p25NetworkModeHang;
#if defined(USE_P25)
	bool         m_p25NetworkDebug;
#endif

#if defined(USE_NXDN)
	bool         m_nxdnNetworkEnabled;
	std::string  m_nxdnNetworkProtocol;
	std::string  m_nxdnGatewayAddress;
	unsigned short m_nxdnGatewayPort;
	std::string  m_nxdnLocalAddress;
	unsigned short m_nxdnLocalPort;
#endif
	unsigned int m_nxdnNetworkModeHang;
#if defined(USE_NXDN)
	bool         m_nxdnNetworkDebug;
#endif

#if defined(USE_POCSAG)
	bool         m_pocsagNetworkEnabled;
	std::string  m_pocsagGatewayAddress;
	unsigned short m_pocsagGatewayPort;
	std::string  m_pocsagLocalAddress;
	unsigned short m_pocsagLocalPort;
	unsigned int m_pocsagNetworkModeHang;
	bool         m_pocsagNetworkDebug;
#endif

#if defined(USE_FM)
	bool         m_fmNetworkEnabled;
	std::string  m_fmGatewayAddress;
	unsigned short m_fmGatewayPort;
	std::string  m_fmLocalAddress;
	unsigned short m_fmLocalPort;
	bool         m_fmPreEmphasis;
	bool         m_fmDeEmphasis;
	float        m_fmTXAudioGain;
	float        m_fmRXAudioGain;
#endif
	unsigned int m_fmNetworkModeHang;
#if defined(USE_FM)
	bool         m_fmNetworkDebug;
#endif

	bool         m_lockFileEnabled;
	std::string  m_lockFileName;

	bool         m_remoteControlEnabled;
};

#endif

