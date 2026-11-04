/*
 *   Copyright (C) 2011-2018,2020,2021,2023,2025,2026 by Jonathan Naylor G4KLX
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

#ifndef	Modem_H
#define	Modem_H

#include "ModemPort.h"
#include "RingBuffer.h"
#include "Defines.h"
#include "Timer.h"
#include "DMRCSBK.h"
#include "DMRSlotType.h"
#include "Sync.h"

#include <string>

enum class RESP_TYPE_MMDVM {
	OK,
	TIMEOUT,
	ERR
};

enum class SERIAL_STATE {
	START,
	LENGTH1,
	LENGTH2,
	TYPE,
	DATA
};

class CModem {
public:
	CModem(bool duplex, bool rxInvert, bool txInvert, bool pttInvert, unsigned int txDelay, unsigned int dmrDelay, bool useCOSAsLockout, bool trace, bool debug);
	~CModem();

	void setPort(IModemPort* port);
	void setRFParams(unsigned int rxFrequency, int rxOffset, unsigned int txFrequency, int txOffset, int txDCOffset, int rxDCOffset, float rfLevel, unsigned int pocsagFrequency);
	void setModeParams(bool dstarEnabled, bool dmrEnabled, bool ysfEnabled, bool p25Enabled, bool nxdnEnabled, bool pocsagEnabled, bool fmEnabled);
	void setLevels(float rxLevel, float cwIdTXLevel, float dstarTXLevel, float dmrTXLevel, float ysfTXLevel, float p25TXLevel, float nxdnTXLevel, float pocsagLevel, float fmTXLevel);

#if defined(USE_DMR)
	void setDMRParams(unsigned int colorCode);
	void setDMRTrunkingParams(bool controlChannel);
#endif
#if defined(USE_YSF)
	void setYSFParams(bool loDev, unsigned int txHang);
#endif
#if defined(USE_P25)
	void setP25Params(unsigned int txHang);
#endif
#if defined(USE_NXDN)
	void setNXDNParams(unsigned int txHang);
#endif
	void setTransparentDataParams(unsigned int sendFrameType);

#if defined(USE_FM)
	void setFMCallsignParams(const std::string& callsign, unsigned int callsignSpeed, unsigned int callsignFrequency, unsigned int callsignTime, unsigned int callsignHoldoff, float callsignHighLevel, float callsignLowLevel, bool callsignAtStart, bool callsignAtEnd, bool callsignAtLatch);
	void setFMAckParams(const std::string& rfAck, unsigned int ackSpeed, unsigned int ackFrequency, unsigned int ackMinTime, unsigned int ackDelay, float ackLevel);
	void setFMMiscParams(unsigned int timeout, float timeoutLevel, float ctcssFrequency, unsigned int ctcssHighThreshold, unsigned int ctcssLowThreshold, float ctcssLevel, unsigned int kerchunkTime, unsigned int hangTime, unsigned int accessMode, bool linkMode, bool cosInvert, bool noiseSquelch, unsigned int squelchHighThreshold, unsigned int squelchLowThreshold, unsigned int rfAudioBoost, float maxDevLevel);
	void setFMExtParams(const std::string& ack, unsigned int audioBoost);
#endif

	bool open();

	bool hasDStar() const;
	bool hasDMR() const;
	bool hasYSF() const;
	bool hasP25() const;
	bool hasNXDN() const;
	bool hasPOCSAG() const;
	bool hasFM() const;

	unsigned int getVersion() const;

#if defined(USE_DSTAR)
	unsigned int readDStarData(unsigned char* data);
#endif
#if defined(USE_DMR)
	unsigned int readDMRData1(unsigned char* data);
	unsigned int readDMRData2(unsigned char* data);
#endif
#if defined(USE_YSF)
	unsigned int readYSFData(unsigned char* data);
#endif
#if defined(USE_P25)
	unsigned int readP25Data(unsigned char* data);
#endif
#if defined(USE_NXDN)
	unsigned int readNXDNData(unsigned char* data);
#endif
#if defined(USE_FM)
	unsigned int readFMData(unsigned char* data);
#endif

#if defined(USE_DSTAR)
	bool hasDStarSpace() const;
#endif
#if defined(USE_DMR)
	bool hasDMRSpace1() const;
	bool hasDMRSpace2() const;
#endif
#if defined(USE_YSF)
	bool hasYSFSpace() const;
#endif
#if defined(USE_P25)
	bool hasP25Space() const;
#endif
#if defined(USE_NXDN)
	bool hasNXDNSpace() const;
#endif
#if defined(USE_POCSAG)
	bool hasPOCSAGSpace() const;
#endif
#if defined(USE_FM)
	unsigned int getFMSpace() const;
#endif

	bool hasTX() const;
	bool hasCD() const;

	bool hasLockout() const;
	bool hasError() const;

	bool writeConfig();

#if defined(USE_DSTAR)
	bool writeDStarData(const unsigned char* data, unsigned int length);
#endif
#if defined(USE_DMR)
	bool writeDMRData1(const unsigned char* data, unsigned int length);
	bool writeDMRData2(const unsigned char* data, unsigned int length);
#endif
#if defined(USE_YSF)
	bool writeYSFData(const unsigned char* data, unsigned int length);
#endif
#if defined(USE_P25)
	bool writeP25Data(const unsigned char* data, unsigned int length);
#endif
#if defined(USE_NXDN)
	bool writeNXDNData(const unsigned char* data, unsigned int length);
#endif
#if defined(USE_POCSAG)
	bool writePOCSAGData(const unsigned char* data, unsigned int length);
#endif
#if defined(USE_FM)
	bool writeFMData(const unsigned char* data, unsigned int length);
#endif

#if defined(USE_DSTAR)
	bool writeDStarInfo(const char* my1, const char* my2, const char* your, const char* type, const char* reflector);
#endif
#if defined(USE_DMR)
	bool writeDMRInfo(unsigned int slotNo, const std::string& src, bool group, const std::string& dst, const char* type);
#endif
#if defined(USE_YSF)
	bool writeYSFInfo(const char* source, const char* dest, unsigned char dgid, const char* type, const char* origin);
#endif
#if defined(USE_P25)
	bool writeP25Info(const char* source, bool group, unsigned int dest, const char* type);
#endif
#if defined(USE_NXDN)
	bool writeNXDNInfo(const char* source, bool group, unsigned int dest, const char* type);
#endif
#if defined(USE_POCSAG)
	bool writePOCSAGInfo(unsigned int ric, const std::string& message);
#endif
	bool writeIPInfo(const std::string& address);

#if defined(USE_DMR)
	bool writeDMRStart(bool tx);
	bool writeDMRAloha(unsigned int systemCode, bool registrationRequired, bool alternateSlot);
	void setDMRShortLC(unsigned int systemCode, bool isControlChannel, bool registrationRequired);
	bool writeDMRShortLC(const unsigned char* lc, bool control);
	bool writeDMRAbort(unsigned int slotNo);
#endif
	bool writeTransparentData(const unsigned char* data, unsigned int length);
	unsigned int readTransparentData(unsigned char* data);

	bool writeSerialData(const unsigned char* data, unsigned int length);
	bool writeQSOInfo(const unsigned char* data, unsigned int length);
	unsigned int readSerialData(unsigned char* data);

	unsigned char getMode() const;
	bool setMode(unsigned char mode);

	bool sendCWId(const std::string& callsign);

	// Listen-only: nothing is transmitted while set.
	void setTXInhibit(bool inhibit);

	void clock(unsigned int ms);

	void close();

#if defined(USE_DMR)
	bool getDMRTrunkingEnabled() const;
	bool getDMRControlChannel() const;
#endif

private:
	unsigned int               m_protocolVersion;
#if defined(USE_DMR)
	unsigned int               m_dmrColorCode;
#endif
#if defined(USE_YSF)
	bool                       m_ysfLoDev;
	unsigned int               m_ysfTXHang;
#endif
#if defined(USE_P25)
	unsigned int               m_p25TXHang;
#endif
#if defined(USE_NXDN)
	unsigned int               m_nxdnTXHang;
#endif
	bool                       m_duplex;
	bool                       m_rxInvert;
	bool                       m_txInvert;
	bool                       m_pttInvert;
	unsigned int               m_txDelay;
#if defined(USE_DMR)
	unsigned int               m_dmrDelay;
#endif
	float                      m_rxLevel;
	float                      m_cwIdTXLevel;
#if defined(USE_DSTAR)
	float                      m_dstarTXLevel;
#endif
#if defined(USE_DMR)
	float                      m_dmrTXLevel;
#endif
#if defined(USE_YSF)
	float                      m_ysfTXLevel;
#endif
#if defined(USE_P25)
	float                      m_p25TXLevel;
#endif
#if defined(USE_NXDN)
	float                      m_nxdnTXLevel;
#endif
#if defined(USE_POCSAG)
	float                      m_pocsagTXLevel;
#endif
#if defined(USE_FM)
	float                      m_fmTXLevel;
#endif
	float                      m_rfLevel;
	bool                       m_useCOSAsLockout;
	bool                       m_trace;
	bool                       m_debug;
	unsigned int               m_rxFrequency;
	unsigned int               m_txFrequency;
#if defined(USE_POCSAG)
	unsigned int               m_pocsagFrequency;
#endif
#if defined(USE_DSTAR)
	bool                       m_dstarEnabled;
#endif
#if defined(USE_DMR)
	bool                       m_dmrEnabled;
	bool                       m_dmrTrunkingEnabled;
	bool                       m_dmrControlChannel;
#endif
#if defined(USE_YSF)
	bool                       m_ysfEnabled;
#endif
#if defined(USE_P25)
	bool                       m_p25Enabled;
#endif
#if defined(USE_NXDN)
	bool                       m_nxdnEnabled;
#endif
#if defined(USE_POCSAG)
	bool                       m_pocsagEnabled;
#endif
#if defined(USE_FM)
	bool                       m_fmEnabled;
#endif
	int                        m_rxDCOffset;
	int                        m_txDCOffset;
	IModemPort*                m_port;
	unsigned char*             m_buffer;
	unsigned int               m_length;
	unsigned int               m_offset;
	SERIAL_STATE               m_state;
	unsigned char              m_type;

#if defined(USE_DSTAR)
	CRingBuffer<unsigned char> m_rxDStarData;
	CRingBuffer<unsigned char> m_txDStarData;
#endif
#if defined(USE_DMR)
	CRingBuffer<unsigned char> m_rxDMRData1;
	CRingBuffer<unsigned char> m_rxDMRData2;
	CRingBuffer<unsigned char> m_txDMRData1;
	CRingBuffer<unsigned char> m_txDMRData2;
#endif
#if defined(USE_YSF)
	CRingBuffer<unsigned char> m_rxYSFData;
	CRingBuffer<unsigned char> m_txYSFData;
#endif
#if defined(USE_P25)
	CRingBuffer<unsigned char> m_rxP25Data;
	CRingBuffer<unsigned char> m_txP25Data;
#endif
#if defined(USE_NXDN)
	CRingBuffer<unsigned char> m_rxNXDNData;
	CRingBuffer<unsigned char> m_txNXDNData;
#endif
#if defined(USE_POCSAG)
	CRingBuffer<unsigned char> m_txPOCSAGData;
#endif
#if defined(USE_FM)
	CRingBuffer<unsigned char> m_rxFMData;
	CRingBuffer<unsigned char> m_txFMData;
#endif
	CRingBuffer<unsigned char> m_rxSerialData;
	CRingBuffer<unsigned char> m_txSerialData;
	CRingBuffer<unsigned char> m_rxTransparentData;
	CRingBuffer<unsigned char> m_txTransparentData;
	unsigned int               m_sendTransparentDataFrameType;
	CTimer                     m_statusTimer;
	CTimer                     m_inactivityTimer;
	CTimer                     m_playoutTimer;
#if defined(USE_DSTAR)
	unsigned int               m_dstarSpace;
#endif
#if defined(USE_DMR)
	unsigned int               m_dmrSpace1;
	unsigned int               m_dmrSpace2;
#endif
#if defined(USE_YSF)
	unsigned int               m_ysfSpace;
#endif
#if defined(USE_P25)
	unsigned int               m_p25Space;
#endif
#if defined(USE_NXDN)
	unsigned int               m_nxdnSpace;
#endif
#if defined(USE_POCSAG)
	unsigned int               m_pocsagSpace;
#endif
#if defined(USE_FM)
	unsigned int               m_fmSpace;
#endif
	bool                       m_tx;
	bool                       m_txInhibit;
	bool                       m_cd;
	bool                       m_lockout;
	bool                       m_error;
	unsigned char              m_mode;
#if defined(USE_FM)
	std::string                m_fmCallsign;
	unsigned int               m_fmCallsignSpeed;
	unsigned int               m_fmCallsignFrequency;
	unsigned int               m_fmCallsignTime;
	unsigned int               m_fmCallsignHoldoff;
	float                      m_fmCallsignHighLevel;
	float                      m_fmCallsignLowLevel;
	bool                       m_fmCallsignAtStart;
	bool                       m_fmCallsignAtEnd;
	bool                       m_fmCallsignAtLatch;
	std::string                m_fmRfAck;
	std::string                m_fmExtAck;
	unsigned int               m_fmAckSpeed;
	unsigned int               m_fmAckFrequency;
	unsigned int               m_fmAckMinTime;
	unsigned int               m_fmAckDelay;
	float                      m_fmAckLevel;
	unsigned int               m_fmTimeout;
	float                      m_fmTimeoutLevel;
	float                      m_fmCtcssFrequency;
	unsigned int               m_fmCtcssHighThreshold;
	unsigned int               m_fmCtcssLowThreshold;
	float                      m_fmCtcssLevel;
	unsigned int               m_fmKerchunkTime;
	unsigned int               m_fmHangTime;
	unsigned int               m_fmAccessMode;
	bool                       m_fmLinkMode;
	bool                       m_fmCOSInvert;
	bool                       m_fmNoiseSquelch;
	unsigned int               m_fmSquelchHighThreshold;
	unsigned int               m_fmSquelchLowThreshold;
	unsigned int               m_fmRFAudioBoost;
	unsigned int               m_fmExtAudioBoost;
	float                      m_fmMaxDevLevel;
	bool                       m_fmExtEnable;
#endif
	unsigned char              m_capabilities1;
	unsigned char              m_capabilities2;

	bool readVersion();
	bool readStatus();
	bool setConfig1();
	bool setConfig2();
	bool setFrequency();
#if defined(USE_FM)
	bool setFMCallsignParams();
	bool setFMAckParams();
	bool setFMMiscParams();
	bool setFMExtParams();
#endif
	void printDebug();

	RESP_TYPE_MMDVM getResponse();

	// Added these for buffering serial data from display:
    unsigned char m_serialDataBuffer[256];
    unsigned int  m_serialDataLen;
};

#endif
