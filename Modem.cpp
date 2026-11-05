/*
 *   Copyright (C) 2011-2018,2020,2021,2023,2025,2026 by Jonathan Naylor G4KLX
 *   Copyright (C) 2026 by Adrian Musceac YO8RZZ
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

#include "DStarDefines.h"
#include "DMRDefines.h"
#include "YSFDefines.h"
#include "P25Defines.h"
#include "NXDNDefines.h"
#include "POCSAGDefines.h"
#include "Thread.h"
#include "Modem.h"
#include "Utils.h"
#include "Log.h"
#include "CRC.h"
#include "DMRShortLC.h"

#include <cmath>
#include <cstdio>
#include <cassert>
#include <cstdint>
#include <ctime>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <unistd.h>
#endif

const unsigned char MMDVM_FRAME_START = 0xE0U;

const unsigned char MMDVM_GET_VERSION = 0x00U;
const unsigned char MMDVM_GET_STATUS  = 0x01U;
const unsigned char MMDVM_SET_CONFIG  = 0x02U;
const unsigned char MMDVM_SET_MODE    = 0x03U;
const unsigned char MMDVM_SET_FREQ    = 0x04U;

const unsigned char MMDVM_SEND_CWID   = 0x0AU;

#if defined(USE_DSTAR)
const unsigned char MMDVM_DSTAR_HEADER = 0x10U;
const unsigned char MMDVM_DSTAR_DATA   = 0x11U;
const unsigned char MMDVM_DSTAR_LOST   = 0x12U;
const unsigned char MMDVM_DSTAR_EOT    = 0x13U;
#endif

#if defined(USE_DMR)
const unsigned char MMDVM_DMR_ALOHA   = 0x17U;
const unsigned char MMDVM_DMR_DATA1   = 0x18U;
const unsigned char MMDVM_DMR_LOST1   = 0x19U;
const unsigned char MMDVM_DMR_DATA2   = 0x1AU;
const unsigned char MMDVM_DMR_LOST2   = 0x1BU;
const unsigned char MMDVM_DMR_SHORTLC = 0x1CU;
const unsigned char MMDVM_DMR_START   = 0x1DU;
const unsigned char MMDVM_DMR_ABORT   = 0x1EU;
#endif

#if defined(USE_YSF)
const unsigned char MMDVM_YSF_DATA    = 0x20U;
const unsigned char MMDVM_YSF_LOST    = 0x21U;
#endif

#if defined(USE_P25)
const unsigned char MMDVM_P25_HDR     = 0x30U;
const unsigned char MMDVM_P25_LDU     = 0x31U;
const unsigned char MMDVM_P25_LOST    = 0x32U;
#endif

#if defined(USE_NXDN)
const unsigned char MMDVM_NXDN_DATA   = 0x40U;
const unsigned char MMDVM_NXDN_LOST   = 0x41U;
#endif

#if defined(USE_POCSAG)
const unsigned char MMDVM_POCSAG_DATA = 0x50U;
#endif

#if defined(USE_FM)
const unsigned char MMDVM_FM_PARAMS1  = 0x60U;
const unsigned char MMDVM_FM_PARAMS2  = 0x61U;
const unsigned char MMDVM_FM_PARAMS3  = 0x62U;
const unsigned char MMDVM_FM_PARAMS4  = 0x63U;
const unsigned char MMDVM_FM_DATA     = 0x65U;
const unsigned char MMDVM_FM_STATUS   = 0x66U;
const unsigned char MMDVM_FM_EOT      = 0x67U;
const unsigned char MMDVM_FM_RSSI     = 0x68U;
#endif

const unsigned char MMDVM_ACK         = 0x70U;
const unsigned char MMDVM_NAK         = 0x7FU;

const unsigned char MMDVM_SERIAL_DATA = 0x80U;

const unsigned char MMDVM_TRANSPARENT = 0x90U;
const unsigned char MMDVM_QSO_INFO    = 0x91U;

const unsigned char MMDVM_DEBUG1      = 0xF1U;
const unsigned char MMDVM_DEBUG2      = 0xF2U;
const unsigned char MMDVM_DEBUG3      = 0xF3U;
const unsigned char MMDVM_DEBUG4      = 0xF4U;
const unsigned char MMDVM_DEBUG5      = 0xF5U;
const unsigned char MMDVM_DEBUG_DUMP  = 0xFAU;

const unsigned int MAX_RESPONSES = 30U;

const unsigned int BUFFER_LENGTH = 2000U;

const unsigned char CAP1_DSTAR  = 0x01U;
const unsigned char CAP1_DMR    = 0x02U;
const unsigned char CAP1_YSF    = 0x04U;
const unsigned char CAP1_P25    = 0x08U;
const unsigned char CAP1_NXDN   = 0x10U;
const unsigned char CAP1_FM     = 0x40U;
const unsigned char CAP2_POCSAG = 0x01U;

CModem::CModem(bool duplex, bool rxInvert, bool txInvert, bool pttInvert, unsigned int txDelay, unsigned int dmrDelay, bool useCOSAsLockout, bool trace, bool debug) :
m_protocolVersion(0U),
#if defined(USE_DMR)
m_dmrColorCode(0U),
#endif
#if defined(USE_YSF)
m_ysfLoDev(false),
m_ysfTXHang(4U),
#endif
#if defined(USE_P25)
m_p25TXHang(5U),
#endif
#if defined(USE_NXDN)
m_nxdnTXHang(5U),
#endif
m_duplex(duplex),
m_rxInvert(rxInvert),
m_txInvert(txInvert),
m_pttInvert(pttInvert),
m_txDelay(txDelay),
#if defined(USE_DMR)
m_dmrDelay(dmrDelay),
#endif
m_rxLevel(0.0F),
m_cwIdTXLevel(0.0F),
#if defined(USE_DSTAR)
m_dstarTXLevel(0.0F),
#endif
#if defined(USE_DMR)
m_dmrTXLevel(0.0F),
#endif
#if defined(USE_YSF)
m_ysfTXLevel(0.0F),
#endif
#if defined(USE_P25)
m_p25TXLevel(0.0F),
#endif
#if defined(USE_NXDN)
m_nxdnTXLevel(0.0F),
#endif
#if defined(USE_POCSAG)
m_pocsagTXLevel(0.0F),
#endif
#if defined(USE_FM)
m_fmTXLevel(0.0F),
#endif
m_rfLevel(0.0F),
m_useCOSAsLockout(useCOSAsLockout),
m_trace(trace),
m_debug(debug),
m_rxFrequency(0U),
m_txFrequency(0U),
#if defined(USE_POCSAG)
m_pocsagFrequency(0U),
#endif
#if defined(USE_DSTAR)
m_dstarEnabled(false),
#endif
#if defined(USE_DMR)
m_dmrEnabled(false),
m_dmrTrunkingEnabled(false),
m_dmrControlChannel(false),
#endif
#if defined(USE_YSF)
m_ysfEnabled(false),
#endif
#if defined(USE_P25)
m_p25Enabled(false),
#endif
#if defined(USE_NXDN)
m_nxdnEnabled(false),
#endif
#if defined(USE_POCSAG)
m_pocsagEnabled(false),
#endif
#if defined(USE_FM)
m_fmEnabled(false),
#endif
m_rxDCOffset(0),
m_txDCOffset(0),
m_port(nullptr),
m_buffer(nullptr),
m_length(0U),
m_offset(0U),
m_state(SERIAL_STATE::START),
m_type(0U),
#if defined(USE_DSTAR)
m_rxDStarData(1000U, "Modem RX D-Star"),
m_txDStarData(1000U, "Modem TX D-Star"),
#endif
#if defined(USE_DMR)
m_rxDMRData1(1000U, "Modem RX DMR1"),
m_rxDMRData2(1000U, "Modem RX DMR2"),
m_txDMRData1(1000U, "Modem TX DMR1"),
m_txDMRData2(1000U, "Modem TX DMR2"),
#endif
#if defined(USE_YSF)
m_rxYSFData(1000U, "Modem RX YSF"),
m_txYSFData(1000U, "Modem TX YSF"),
#endif
#if defined(USE_P25)
m_rxP25Data(1000U, "Modem RX P25"),
m_txP25Data(1000U, "Modem TX P25"),
#endif
#if defined(USE_NXDN)
m_rxNXDNData(1000U, "Modem RX NXDN"),
m_txNXDNData(1000U, "Modem TX NXDN"),
#endif
#if defined(USE_POCSAG)
m_txPOCSAGData(1000U, "Modem TX POCSAG"),
#endif
#if defined(USE_FM)
m_rxFMData(5000U, "Modem RX FM"),
m_txFMData(5000U, "Modem TX FM"),
#endif
m_rxSerialData(1000U, "Modem RX Serial"),
m_txSerialData(1000U, "Modem TX Serial"),
m_rxTransparentData(1000U, "Modem RX Transparent"),
m_txTransparentData(1000U, "Modem TX Transparent"),
m_sendTransparentDataFrameType(0U),
m_statusTimer(1000U, 0U, 250U),
m_inactivityTimer(1000U, 2U),
m_playoutTimer(1000U, 0U, 10U),
#if defined(USE_DSTAR)
m_dstarSpace(0U),
#endif
#if defined(USE_DMR)
m_dmrSpace1(0U),
m_dmrSpace2(0U),
#endif
#if defined(USE_YSF)
m_ysfSpace(0U),
#endif
#if defined(USE_P25)
m_p25Space(0U),
#endif
#if defined(USE_NXDN)
m_nxdnSpace(0U),
#endif
#if defined(USE_POCSAG)
m_pocsagSpace(0U),
#endif
#if defined(USE_FM)
m_fmSpace(0U),
#endif
m_tx(false),
m_txInhibit(false),
m_cd(false),
m_lockout(false),
m_error(false),
m_mode(MODE_IDLE),
#if defined(USE_FM)
m_fmCallsign(),
m_fmCallsignSpeed(20U),
m_fmCallsignFrequency(1000U),
m_fmCallsignTime(600U),
m_fmCallsignHoldoff(0U),
m_fmCallsignHighLevel(35.0F),
m_fmCallsignLowLevel(15.0F),
m_fmCallsignAtStart(true),
m_fmCallsignAtEnd(true),
m_fmCallsignAtLatch(true),
m_fmRfAck("K"),
m_fmExtAck("N"),
m_fmAckSpeed(20U),
m_fmAckFrequency(1750U),
m_fmAckMinTime(4U),
m_fmAckDelay(1000U),
m_fmAckLevel(80.0F),
m_fmTimeout(120U),
m_fmTimeoutLevel(80.0F),
m_fmCtcssFrequency(88.4F),
m_fmCtcssHighThreshold(30U),
m_fmCtcssLowThreshold(20U),
m_fmCtcssLevel(10.0F),
m_fmKerchunkTime(0U),
m_fmHangTime(5U),
m_fmAccessMode(1U),
m_fmLinkMode(false),
m_fmCOSInvert(false),
m_fmNoiseSquelch(false),
m_fmSquelchHighThreshold(30U),
m_fmSquelchLowThreshold(20U),
m_fmRFAudioBoost(1U),
m_fmExtAudioBoost(1U),
m_fmMaxDevLevel(90.0F),
m_fmExtEnable(false),
#endif
m_capabilities1(0x00U),
m_capabilities2(0x00U),
m_serialDataLen(0U)
{
	m_buffer = new unsigned char[BUFFER_LENGTH];
}

CModem::~CModem()
{
	delete   m_port;
	delete[] m_buffer;
}

void CModem::setPort(IModemPort* port)
{
	assert(port != nullptr);

	m_port = port;
}

void CModem::setRFParams(unsigned int rxFrequency, int rxOffset, unsigned int txFrequency, int txOffset, int txDCOffset, int rxDCOffset, float rfLevel, unsigned int pocsagFrequency)
{
	m_rxFrequency     = rxFrequency + rxOffset;
	m_txFrequency     = txFrequency + txOffset;
	m_txDCOffset      = txDCOffset;
	m_rxDCOffset      = rxDCOffset;
	m_rfLevel         = rfLevel;
#if defined(USE_POCSAG)
	m_pocsagFrequency = pocsagFrequency + txOffset;
#endif
}

void CModem::setModeParams(bool dstarEnabled, bool dmrEnabled, bool ysfEnabled, bool p25Enabled, bool nxdnEnabled, bool pocsagEnabled, bool fmEnabled)
{
#if defined(USE_DSTAR)
	m_dstarEnabled  = dstarEnabled;
#endif
#if defined(USE_DMR)
	m_dmrEnabled    = dmrEnabled;
#endif
#if defined(USE_YSF)
	m_ysfEnabled    = ysfEnabled;
#endif
#if defined(USE_P25)
	m_p25Enabled    = p25Enabled;
#endif
#if defined(USE_NXDN)
	m_nxdnEnabled   = nxdnEnabled;
#endif
#if defined(USE_POCSAG)
	m_pocsagEnabled = pocsagEnabled;
#endif
#if defined(USE_FM)
	m_fmEnabled     = fmEnabled;
#endif
}

void CModem::setLevels(float rxLevel, float cwIdTXLevel, float dstarTXLevel, float dmrTXLevel, float ysfTXLevel, float p25TXLevel, float nxdnTXLevel, float pocsagTXLevel, float fmTXLevel)
{
	m_rxLevel       = rxLevel;
	m_cwIdTXLevel   = cwIdTXLevel;
#if defined(USE_DSTAR)
	m_dstarTXLevel  = dstarTXLevel;
#endif
#if defined(USE_DMR)
	m_dmrTXLevel    = dmrTXLevel;
#endif
#if defined(USE_YSF)
	m_ysfTXLevel    = ysfTXLevel;
#endif
#if defined(USE_P25)
	m_p25TXLevel    = p25TXLevel;
#endif
#if defined(USE_NXDN)
	m_nxdnTXLevel   = nxdnTXLevel;
#endif
#if defined(USE_POCSAG)
	m_pocsagTXLevel = pocsagTXLevel;
#endif
#if defined(USE_FM)
	m_fmTXLevel     = fmTXLevel;
#endif
}

#if defined(USE_DMR)
void CModem::setDMRParams(unsigned int colorCode)
{
	assert(colorCode < 16U);

	m_dmrColorCode = colorCode;
}

void CModem::setDMRTrunkingParams(bool controlChannel)
{
	m_dmrTrunkingEnabled = true;
	m_dmrControlChannel  = controlChannel;
}
#endif

#if defined(USE_YSF)
void CModem::setYSFParams(bool loDev, unsigned int txHang)
{
	m_ysfLoDev  = loDev;
	m_ysfTXHang = txHang;
}
#endif

#if defined(USE_P25)
void CModem::setP25Params(unsigned int txHang)
{
	m_p25TXHang = txHang;
}
#endif

#if defined(USE_NXDN)
void CModem::setNXDNParams(unsigned int txHang)
{
	m_nxdnTXHang = txHang;
}
#endif

void CModem::setTransparentDataParams(unsigned int sendFrameType)
{
    m_sendTransparentDataFrameType = sendFrameType;
}

bool CModem::open()
{
	::LogMessage("Opening the MMDVM");

	bool ret = m_port->open();
	if (!ret)
		return false;

	ret = readVersion();
	if (!ret) {
		m_port->close();
		delete m_port;
		m_port = nullptr;
		return false;
	} else {
		/* Stopping the inactivity timer here when a firmware version has been
		   successfuly read prevents the death spiral of "no reply from modem..." */
		m_inactivityTimer.stop();
	}

	ret = setFrequency();
	if (!ret) {
		m_port->close();
		delete m_port;
		m_port = nullptr;
		return false;
	}

	ret = writeConfig();
	if (!ret) {
		m_port->close();
		delete m_port;
		m_port = nullptr;
		return false;
	}

#if defined(USE_FM)
	if (m_fmEnabled) {
		ret = setFMCallsignParams();
		if (!ret) {
			m_port->close();
			delete m_port;
			m_port = nullptr;
			return false;
		}

		ret = setFMAckParams();
		if (!ret) {
			m_port->close();
			delete m_port;
			m_port = nullptr;
			return false;
		}

		ret = setFMMiscParams();
		if (!ret) {
			m_port->close();
			delete m_port;
			m_port = nullptr;
			return false;
		}

		if (m_fmExtEnable) {
			ret = setFMExtParams();
			if (!ret) {
				m_port->close();
				delete m_port;
				m_port = nullptr;
				return false;
			}
		}
	}
#endif

	m_statusTimer.start();

	m_error  = false;
	m_offset = 0U;

	return true;
}

void CModem::clock(unsigned int ms)
{
	assert(m_port != nullptr);

	// Poll the modem status every 250ms
	m_statusTimer.clock(ms);
	if (m_statusTimer.hasExpired()) {
		readStatus();
		m_statusTimer.start();
	}

	m_inactivityTimer.clock(ms);
	if (m_inactivityTimer.hasExpired()) {
		LogError("No reply from the modem for some time, resetting it");
		m_error = true;
		close();

		CThread::sleep(2000U);		// 2s
		while (!open())
			CThread::sleep(5000U);	// 5s
	}

	RESP_TYPE_MMDVM type = getResponse();

	if (type == RESP_TYPE_MMDVM::TIMEOUT) {
		// Nothing to do
	} else if (type == RESP_TYPE_MMDVM::ERR) {
		// Nothing to do
	} else {
		// type == OK
		switch (m_type) {
#if defined(USE_DSTAR)
			case MMDVM_DSTAR_HEADER: {
					if (m_trace)
						CUtils::dump(1U, "RX D-Star Header", m_buffer, m_length);

					unsigned char data = m_length - m_offset + 1U;
					m_rxDStarData.addData(&data, 1U);

					data = TAG_HEADER;
					m_rxDStarData.addData(&data, 1U);

					m_rxDStarData.addData(m_buffer + m_offset, m_length - m_offset);
				}
				break;

			case MMDVM_DSTAR_DATA: {
					if (m_trace)
						CUtils::dump(1U, "RX D-Star Data", m_buffer, m_length);

					unsigned char data = m_length - m_offset + 1U;
					m_rxDStarData.addData(&data, 1U);

					data = TAG_DATA;
					m_rxDStarData.addData(&data, 1U);

					m_rxDStarData.addData(m_buffer + m_offset, m_length - m_offset);
				}
				break;

			case MMDVM_DSTAR_LOST: {
					if (m_trace)
						CUtils::dump(1U, "RX D-Star Lost", m_buffer, m_length);

					unsigned char data = 1U;
					m_rxDStarData.addData(&data, 1U);

					data = TAG_LOST;
					m_rxDStarData.addData(&data, 1U);
				}
				break;

			case MMDVM_DSTAR_EOT: {
					if (m_trace)
						CUtils::dump(1U, "RX D-Star EOT", m_buffer, m_length);

					unsigned char data = 1U;
					m_rxDStarData.addData(&data, 1U);

					data = TAG_EOT;
					m_rxDStarData.addData(&data, 1U);
				}
				break;
#endif

#if defined(USE_DMR)
			case MMDVM_DMR_DATA1: {
					if (m_trace)
						CUtils::dump(1U, "RX DMR Data 1", m_buffer, m_length);

					unsigned char data = m_length - m_offset + 1U;
					m_rxDMRData1.addData(&data, 1U);

					if (m_buffer[3U] == (DMR_SYNC_DATA | DT_TERMINATOR_WITH_LC))
						data = TAG_EOT;
					else
						data = TAG_DATA;
					m_rxDMRData1.addData(&data, 1U);

					m_rxDMRData1.addData(m_buffer + m_offset, m_length - m_offset);
				}
				break;

			case MMDVM_DMR_DATA2: {
					if (m_trace)
						CUtils::dump(1U, "RX DMR Data 2", m_buffer, m_length);

					unsigned char data = m_length - m_offset + 1U;
					m_rxDMRData2.addData(&data, 1U);

					if (m_buffer[3U] == (DMR_SYNC_DATA | DT_TERMINATOR_WITH_LC))
						data = TAG_EOT;
					else
						data = TAG_DATA;
					m_rxDMRData2.addData(&data, 1U);

					m_rxDMRData2.addData(m_buffer + m_offset, m_length - m_offset);
				}
				break;

			case MMDVM_DMR_LOST1: {
					if (m_trace)
						CUtils::dump(1U, "RX DMR Lost 1", m_buffer, m_length);

					unsigned char data = 1U;
					m_rxDMRData1.addData(&data, 1U);

					data = TAG_LOST;
					m_rxDMRData1.addData(&data, 1U);
				}
				break;

			case MMDVM_DMR_LOST2: {
					if (m_trace)
						CUtils::dump(1U, "RX DMR Lost 2", m_buffer, m_length);

					unsigned char data = 1U;
					m_rxDMRData2.addData(&data, 1U);

					data = TAG_LOST;
					m_rxDMRData2.addData(&data, 1U);
				}
				break;
#endif

#if defined(USE_YSF)
			case MMDVM_YSF_DATA: {
					if (m_trace)
						CUtils::dump(1U, "RX YSF Data", m_buffer, m_length);

					unsigned char data = m_length - m_offset + 1U;
					m_rxYSFData.addData(&data, 1U);

					data = TAG_DATA;
					m_rxYSFData.addData(&data, 1U);

					m_rxYSFData.addData(m_buffer + m_offset, m_length - m_offset);
				}
				break;

			case MMDVM_YSF_LOST: {
					if (m_trace)
						CUtils::dump(1U, "RX YSF Lost", m_buffer, m_length);

					unsigned char data = 1U;
					m_rxYSFData.addData(&data, 1U);

					data = TAG_LOST;
					m_rxYSFData.addData(&data, 1U);
				}
				break;
#endif

#if defined(USE_P25)
			case MMDVM_P25_HDR: {
				if (m_trace)
					CUtils::dump(1U, "RX P25 Header", m_buffer, m_length);

				unsigned char data = m_length - m_offset + 1U;
				m_rxP25Data.addData(&data, 1U);

				data = TAG_HEADER;
				m_rxP25Data.addData(&data, 1U);

				m_rxP25Data.addData(m_buffer + m_offset, m_length - m_offset);
			}
			break;

			case MMDVM_P25_LDU: {
				if (m_trace)
					CUtils::dump(1U, "RX P25 LDU", m_buffer, m_length);

				unsigned char data = m_length - m_offset + 1U;
				m_rxP25Data.addData(&data, 1U);

				data = TAG_DATA;
				m_rxP25Data.addData(&data, 1U);

				m_rxP25Data.addData(m_buffer + m_offset, m_length - m_offset);
			}
			break;

			case MMDVM_P25_LOST: {
				if (m_trace)
					CUtils::dump(1U, "RX P25 Lost", m_buffer, m_length);

				unsigned char data = 1U;
				m_rxP25Data.addData(&data, 1U);

				data = TAG_LOST;
				m_rxP25Data.addData(&data, 1U);
			}
			break;
#endif

#if defined(USE_NXDN)
			case MMDVM_NXDN_DATA: {
				if (m_trace)
					CUtils::dump(1U, "RX NXDN Data", m_buffer, m_length);

				unsigned char data = m_length - m_offset + 1U;
				m_rxNXDNData.addData(&data, 1U);

				data = TAG_DATA;
				m_rxNXDNData.addData(&data, 1U);

				m_rxNXDNData.addData(m_buffer + m_offset, m_length - m_offset);
			}
			break;

			case MMDVM_NXDN_LOST: {
				if (m_trace)
					CUtils::dump(1U, "RX NXDN Lost", m_buffer, m_length);

				unsigned char data = 1U;
				m_rxNXDNData.addData(&data, 1U);

				data = TAG_LOST;
				m_rxNXDNData.addData(&data, 1U);
			}
			break;
#endif

#if defined(USE_FM)
			case MMDVM_FM_DATA: {
				if (m_trace)
					CUtils::dump(1U, "RX FM Data", m_buffer, m_length);

				unsigned int data1 = m_length - m_offset + 1U;
				m_rxFMData.addData((unsigned char*)&data1, sizeof(unsigned int));

				unsigned char data2 = TAG_DATA;
				m_rxFMData.addData(&data2, 1U);

				m_rxFMData.addData(m_buffer + m_offset, m_length - m_offset);
			}
			break;

			case MMDVM_FM_STATUS: {
				if (m_trace)
					CUtils::dump(1U, "RX FM Status", m_buffer, m_length);

				unsigned int data1 = m_length - m_offset + 1U;
				m_rxFMData.addData((unsigned char*)&data1, sizeof(unsigned int));

				unsigned char data2 = TAG_HEADER;
				m_rxFMData.addData(&data2, 1U);

				m_rxFMData.addData(m_buffer + m_offset, m_length - m_offset);
			}
			break;

			case MMDVM_FM_EOT: {
				if(m_trace)
					CUtils::dump(1U, "RX FM End of transmission", m_buffer, m_length);

				unsigned int data1 = m_length - m_offset + 1U;
				m_rxFMData.addData((unsigned char*)&data1, sizeof(unsigned int));

				unsigned char data2 = TAG_EOT;
				m_rxFMData.addData(&data2, 1U);

				m_rxFMData.addData(m_buffer + m_offset, m_length - m_offset);
			}
			break;

			case MMDVM_FM_RSSI: {
				if(m_trace)
					CUtils::dump(1U, "RX FM RSSI", m_buffer, m_length);

				unsigned int data1 = m_length - m_offset + 1U;
				m_rxFMData.addData((unsigned char*)&data1, sizeof(unsigned int));

				unsigned char data2 = TAG_RSSI;
				m_rxFMData.addData(&data2, 1U);

				m_rxFMData.addData(m_buffer + m_offset, m_length - m_offset);
			}
			break;
#endif

			case MMDVM_GET_STATUS:
				// if (m_trace)
				//	CUtils::dump(1U, "GET_STATUS", m_buffer, m_length);

				switch (m_protocolVersion) {
				case 1U: {
						m_mode = m_buffer[m_offset + 1U];

						m_tx = (m_buffer[m_offset + 2U] & 0x01U) == 0x01U;
						bool adcOverflow = (m_buffer[m_offset + 2U] & 0x02U) == 0x02U;
						if (adcOverflow)
							LogError("MMDVM ADC levels have overflowed");
						bool rxOverflow = (m_buffer[m_offset + 2U] & 0x04U) == 0x04U;
						if (rxOverflow)
							LogError("MMDVM RX buffer has overflowed");
						bool txOverflow = (m_buffer[m_offset + 2U] & 0x08U) == 0x08U;
						if (txOverflow)
							LogError("MMDVM TX buffer has overflowed");
						m_lockout = (m_buffer[m_offset + 2U] & 0x10U) == 0x10U;
						bool dacOverflow = (m_buffer[m_offset + 2U] & 0x20U) == 0x20U;
						if (dacOverflow)
							LogError("MMDVM DAC levels have overflowed");
						m_cd = (m_buffer[m_offset + 2U] & 0x40U) == 0x40U;

#if defined(USE_P25)
						m_p25Space    = 0U;
#endif
#if defined(USE_NXDN)
						m_nxdnSpace   = 0U;
#endif
#if defined(USE_POCSAG)
						m_pocsagSpace = 0U;
#endif
#if defined(USE_FM)
						m_fmSpace     = 0U;
#endif
#if defined(USE_DSTAR)
						m_dstarSpace = m_buffer[m_offset + 3U];
#endif
#if defined(USE_DMR)
						m_dmrSpace1  = m_buffer[m_offset + 4U];
						m_dmrSpace2  = m_buffer[m_offset + 5U];
#endif
#if defined(USE_YSF)
						m_ysfSpace   = m_buffer[m_offset + 6U];
#endif
						// The following depend on the version of the firmware
#if defined(USE_P25)
						if (m_length > (m_offset + 7U))
							m_p25Space    = m_buffer[m_offset + 7U];
#endif
#if defined(USE_NXDN)
						if (m_length > (m_offset + 8U))
							m_nxdnSpace   = m_buffer[m_offset + 8U];
#endif
#if defined(USE_POCSAG)
						if (m_length > (m_offset + 9U))
							m_pocsagSpace = m_buffer[m_offset + 9U];
#endif
					}
					break;

				case 2U: {
						m_mode = m_buffer[m_offset + 0U];

						m_tx = (m_buffer[m_offset + 1U] & 0x01U) == 0x01U;
						bool adcOverflow = (m_buffer[m_offset + 1U] & 0x02U) == 0x02U;
						if (adcOverflow)
							LogError("MMDVM ADC levels have overflowed");
						bool rxOverflow = (m_buffer[m_offset + 1U] & 0x04U) == 0x04U;
						if (rxOverflow)
							LogError("MMDVM RX buffer has overflowed");
						bool txOverflow = (m_buffer[m_offset + 1U] & 0x08U) == 0x08U;
						if (txOverflow)
							LogError("MMDVM TX buffer has overflowed");
						m_lockout = (m_buffer[m_offset + 1U] & 0x10U) == 0x10U;
						bool dacOverflow = (m_buffer[m_offset + 1U] & 0x20U) == 0x20U;
						if (dacOverflow)
							LogError("MMDVM DAC levels have overflowed");
						m_cd = (m_buffer[m_offset + 1U] & 0x40U) == 0x40U;

#if defined(USE_DSTAR)
						m_dstarSpace  = m_buffer[m_offset + 3U];
#endif
#if defined(USE_DMR)
						m_dmrSpace1   = m_buffer[m_offset + 4U];
						m_dmrSpace2   = m_buffer[m_offset + 5U];
#endif
#if defined(USE_YSF)
						m_ysfSpace    = m_buffer[m_offset + 6U];
#endif
#if defined(USE_P25)
						m_p25Space    = m_buffer[m_offset + 7U];
#endif
#if defined(USE_NXDN)
						m_nxdnSpace   = m_buffer[m_offset + 8U];
#endif
#if defined(USE_FM)
						m_fmSpace     = m_buffer[m_offset + 10U];
#endif
#if defined(USE_POCSAG)
						m_pocsagSpace = m_buffer[m_offset + 11U];
#endif
					}
					break;

				default:
#if defined(USE_DSTAR)
					m_dstarSpace  = 0U;
#endif
#if defined(USE_DMR)
					m_dmrSpace1   = 0U;
					m_dmrSpace2   = 0U;
#endif
#if defined(USE_YSF)
					m_ysfSpace    = 0U;
#endif
#if defined(USE_P25)
					m_p25Space    = 0U;
#endif
#if defined(USE_NXDN)
					m_nxdnSpace   = 0U;
#endif
#if defined(USE_POCSAG)
					m_pocsagSpace = 0U;
#endif
#if defined(USE_FM)
					m_fmSpace     = 0U;
#endif
					break;
				}

				m_inactivityTimer.start();
				// LogMessage("status=%02X, tx=%d, space=%u,%u,%u,%u,%u,%u,%u,%u lockout=%d, cd=%d", m_buffer[m_offset + 2U], int(m_tx), m_dstarSpace, m_dmrSpace1, m_dmrSpace2, m_ysfSpace, m_p25Space, m_nxdnSpace, m_pocsagSpace, m_fmSpace, int(m_lockout), int(m_cd));
				break;

			case MMDVM_TRANSPARENT: {
					if (m_trace)
						CUtils::dump(1U, "RX Transparent Data", m_buffer, m_length);

					unsigned char offset = m_sendTransparentDataFrameType;
					if (offset > 1U) offset = 1U;
					unsigned char data = m_length - m_offset + offset;
					m_rxTransparentData.addData(&data, 1U);

					m_rxTransparentData.addData(m_buffer + m_offset - offset, m_length - m_offset + offset);
				}
				break;

			// These should not be received, but don't complain if we do
			case MMDVM_GET_VERSION:
			case MMDVM_ACK:
				break;

			case MMDVM_NAK:
				LogWarning("Received a NAK from the MMDVM, command = 0x%02X, reason = %u", m_buffer[m_offset], m_buffer[m_offset + 1U]);
				break;

			case MMDVM_DEBUG1:
			case MMDVM_DEBUG2:
			case MMDVM_DEBUG3:
			case MMDVM_DEBUG4:
			case MMDVM_DEBUG5:
			case MMDVM_DEBUG_DUMP:
				printDebug();
				break;

			case MMDVM_SERIAL_DATA: {
					if (m_trace)
						CUtils::dump(1U, "RX Serial Data", m_buffer, m_length);

					unsigned char data = m_length - m_offset;
					m_rxSerialData.addData(&data, 1U);

					m_rxSerialData.addData(m_buffer + m_offset, m_length - m_offset);
				}
				
				// NEW: Buffer serial data and forward complete commands as transparent data
				{
					// Add received bytes to our accumulation buffer
					for (unsigned int i = 0; i < (m_length - m_offset); i++) {
						if (m_serialDataLen < 256) {
							m_serialDataBuffer[m_serialDataLen++] = m_buffer[m_offset + i];
							
							// Check for Nextion command terminator (0xFF 0xFF 0xFF)
							if (m_serialDataLen >= 3 && 
								m_serialDataBuffer[m_serialDataLen - 3] == 0xFF &&
								m_serialDataBuffer[m_serialDataLen - 2] == 0xFF &&
								m_serialDataBuffer[m_serialDataLen - 1] == 0xFF) {
								
								// We have a complete command
								// Add it to the RX transparent data queue so it will be forwarded to NextionDriver
								// With sendFrameType=1, we need to include the frame type byte
								
								// Create a buffer with frame type byte + command data
								unsigned char frameBuffer[260];
								frameBuffer[0] = 0x90;  // Frame type: transparent data
								::memcpy(frameBuffer + 1, m_serialDataBuffer, m_serialDataLen);
								
								// Add length byte and data with frame type to RX queue
								unsigned char len = m_serialDataLen + 1U;  // +1 for frame type byte
								m_rxTransparentData.addData(&len, 1U);
								m_rxTransparentData.addData(frameBuffer, len);
								
								if (m_trace) {
									CUtils::dump(1U, "Adding button command with frame type to RX Transparent queue", frameBuffer, len);
								}
								
								// Reset buffer for next command
								m_serialDataLen = 0U;
							}
						} else {
							// Buffer overflow, reset
							LogWarning("Serial data buffer overflow, resetting");
							m_serialDataLen = 0U;
						}
					}
				}
				break;

			default:
				LogMessage("Unknown message, type: %02X", m_type);
				CUtils::dump("Buffer dump", m_buffer, m_length);
				break;
		}
	}

	// Only feed data to the modem if the playout timer has expired
	m_playoutTimer.clock(ms);
	if (!m_playoutTimer.hasExpired())
		return;

#if defined(USE_DSTAR)
	if (m_dstarSpace > 1U && !m_txDStarData.isEmpty()) {
		unsigned char buffer[4U];
		m_txDStarData.peek(buffer, 4U);

		if ((buffer[3U] == MMDVM_DSTAR_HEADER && m_dstarSpace > 4U) ||
			(buffer[3U] == MMDVM_DSTAR_DATA   && m_dstarSpace > 1U) ||
			(buffer[3U] == MMDVM_DSTAR_EOT    && m_dstarSpace > 1U)) {
			unsigned char len = 0U;
			m_txDStarData.getData(&len, 1U);
			m_txDStarData.getData(m_buffer, len);

			switch (buffer[3U]) {
			case MMDVM_DSTAR_HEADER:
				if (m_trace)
					CUtils::dump(1U, "TX D-Star Header", m_buffer, len);
				m_dstarSpace -= 4U;
				break;
			case MMDVM_DSTAR_DATA:
				if (m_trace)
					CUtils::dump(1U, "TX D-Star Data", m_buffer, len);
				m_dstarSpace -= 1U;
				break;
			default:
				if (m_trace)
					CUtils::dump(1U, "TX D-Star EOT", m_buffer, len);
				m_dstarSpace -= 1U;
				break;
			}

			int ret = m_port->write(m_buffer, len);
			if (ret != int(len))
				LogWarning("Error when writing D-Star data to the MMDVM");

			m_playoutTimer.start();
		}
	}
#endif

#if defined(USE_DMR)
	if (m_dmrSpace1 > 1U && !m_txDMRData1.isEmpty()) {
		unsigned char len = 0U;
		m_txDMRData1.getData(&len, 1U);
		m_txDMRData1.getData(m_buffer, len);

		if (m_trace)
			CUtils::dump(1U, "TX DMR Data 1", m_buffer, len);

		int ret = m_port->write(m_buffer, len);
		if (ret != int(len))
			LogWarning("Error when writing DMR data to the MMDVM");

		m_playoutTimer.start();

		m_dmrSpace1--;
	}

	if (m_dmrSpace2 > 1U && !m_txDMRData2.isEmpty()) {
		unsigned char len = 0U;
		m_txDMRData2.getData(&len, 1U);
		m_txDMRData2.getData(m_buffer, len);

		if (m_trace)
			CUtils::dump(1U, "TX DMR Data 2", m_buffer, len);

		int ret = m_port->write(m_buffer, len);
		if (ret != int(len))
			LogWarning("Error when writing DMR data to the MMDVM");

		m_playoutTimer.start();

		m_dmrSpace2--;
	}
#endif

#if defined(USE_YSF)
	if (m_ysfSpace > 1U && !m_txYSFData.isEmpty()) {
		unsigned char len = 0U;
		m_txYSFData.getData(&len, 1U);
		m_txYSFData.getData(m_buffer, len);

		if (m_trace)
			CUtils::dump(1U, "TX YSF Data", m_buffer, len);

		int ret = m_port->write(m_buffer, len);
		if (ret != int(len))
			LogWarning("Error when writing YSF data to the MMDVM");

		m_playoutTimer.start();

		m_ysfSpace--;
	}
#endif

#if defined(USE_P25)
	if (m_p25Space > 1U && !m_txP25Data.isEmpty()) {
		unsigned char len = 0U;
		m_txP25Data.getData(&len, 1U);
		m_txP25Data.getData(m_buffer, len);

		if (m_trace) {
			if (m_buffer[2U] == MMDVM_P25_HDR)
				CUtils::dump(1U, "TX P25 HDR", m_buffer, len);
			else
				CUtils::dump(1U, "TX P25 LDU", m_buffer, len);
		}

		int ret = m_port->write(m_buffer, len);
		if (ret != int(len))
			LogWarning("Error when writing P25 data to the MMDVM");

		m_playoutTimer.start();

		m_p25Space--;
	}
#endif

#if defined(USE_NXDN)
	if (m_nxdnSpace > 1U && !m_txNXDNData.isEmpty()) {
		unsigned char len = 0U;
		m_txNXDNData.getData(&len, 1U);
		m_txNXDNData.getData(m_buffer, len);

		if (m_trace)
			CUtils::dump(1U, "TX NXDN Data", m_buffer, len);

		int ret = m_port->write(m_buffer, len);
		if (ret != int(len))
			LogWarning("Error when writing NXDN data to the MMDVM");

		m_playoutTimer.start();

		m_nxdnSpace--;
	}
#endif

#if defined(USE_POCSAG)
	if (m_pocsagSpace > 1U && !m_txPOCSAGData.isEmpty()) {
		unsigned char len = 0U;
		m_txPOCSAGData.getData(&len, 1U);
		m_txPOCSAGData.getData(m_buffer, len);

		if (m_trace)
			CUtils::dump(1U, "TX POCSAG Data", m_buffer, len);

		int ret = m_port->write(m_buffer, len);
		if (ret != int(len))
			LogWarning("Error when writing POCSAG data to the MMDVM");

		m_playoutTimer.start();

		m_pocsagSpace--;
	}
#endif

#if defined(USE_FM)
	if (m_fmSpace > 1U && !m_txFMData.isEmpty()) {
		unsigned int len = 0U;
		m_txFMData.getData((unsigned char*)&len, sizeof(unsigned int));
		m_txFMData.getData(m_buffer, len);

		if (m_trace) {
			if (m_buffer[2U] == MMDVM_FM_STATUS)
				CUtils::dump(1U, "TX FM Status", m_buffer, len);
			else
				CUtils::dump(1U, "TX FM Data", m_buffer, len);
		}

		int ret = m_port->write(m_buffer, len);
		if (ret != int(len))
			LogWarning("Error when writing FM data to the MMDVM");

		m_playoutTimer.start();

		m_fmSpace--;
	}
#endif

	if (!m_txTransparentData.isEmpty()) {
		unsigned char len = 0U;
		m_txTransparentData.getData(&len, 1U);
		m_txTransparentData.getData(m_buffer, len);

		if (m_trace)
			CUtils::dump(1U, "TX Transparent Data", m_buffer, len);

		int ret = m_port->write(m_buffer, len);
		if (ret != int(len))
			LogWarning("Error when writing Transparent data to the MMDVM");
	}

	if (!m_txSerialData.isEmpty()) {
		unsigned char len = 0U;
		m_txSerialData.getData(&len, 1U);
		m_txSerialData.getData(m_buffer, len);

		if (m_trace)
			CUtils::dump(1U, "TX Serial Data", m_buffer, len);

		int ret = m_port->write(m_buffer, len);
		if (ret != int(len))
			LogWarning("Error when writing Serial data to the MMDVM");
	}
}

void CModem::close()
{
	assert(m_port != nullptr);

	::LogMessage("Closing the MMDVM");

	m_port->close();
}

#if defined(USE_DSTAR)
unsigned int CModem::readDStarData(unsigned char* data)
{
	assert(data != nullptr);

	if (m_rxDStarData.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_rxDStarData.getData(&len, 1U);
	m_rxDStarData.getData(data, len);

	return len;
}
#endif

#if defined(USE_DMR)
unsigned int CModem::readDMRData1(unsigned char* data)
{
	assert(data != nullptr);

	if (m_rxDMRData1.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_rxDMRData1.getData(&len, 1U);
	m_rxDMRData1.getData(data, len);

	return len;
}

unsigned int CModem::readDMRData2(unsigned char* data)
{
	assert(data != nullptr);

	if (m_rxDMRData2.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_rxDMRData2.getData(&len, 1U);
	m_rxDMRData2.getData(data, len);

	return len;
}
#endif

#if defined(USE_YSF)
unsigned int CModem::readYSFData(unsigned char* data)
{
	assert(data != nullptr);

	if (m_rxYSFData.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_rxYSFData.getData(&len, 1U);
	m_rxYSFData.getData(data, len);

	return len;
}
#endif

#if defined(USE_P25)
unsigned int CModem::readP25Data(unsigned char* data)
{
	assert(data != nullptr);

	if (m_rxP25Data.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_rxP25Data.getData(&len, 1U);
	m_rxP25Data.getData(data, len);

	return len;
}
#endif

#if defined(USE_NXDN)
unsigned int CModem::readNXDNData(unsigned char* data)
{
	assert(data != nullptr);

	if (m_rxNXDNData.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_rxNXDNData.getData(&len, 1U);
	m_rxNXDNData.getData(data, len);

	return len;
}
#endif

#if defined(USE_FM)
unsigned int CModem::readFMData(unsigned char* data)
{
	assert(data != nullptr);

	if (m_rxFMData.isEmpty())
		return 0U;

	unsigned int len = 0U;
	m_rxFMData.getData((unsigned char*)&len, sizeof(unsigned int));
	m_rxFMData.getData(data, len);

	return len;
}
#endif

unsigned int CModem::readTransparentData(unsigned char* data)
{
	assert(data != nullptr);

	if (m_rxTransparentData.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_rxTransparentData.getData(&len, 1U);
	m_rxTransparentData.getData(data, len);

	return len;
}

unsigned int CModem::readSerialData(unsigned char* data)
{
	assert(data != nullptr);

	if (m_rxSerialData.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_rxSerialData.getData(&len, 1U);
	m_rxSerialData.getData(data, len);

	return len;
}

#if defined(USE_DSTAR)
bool CModem::hasDStarSpace() const
{
	unsigned int space = m_txDStarData.freeSpace() / (DSTAR_FRAME_LENGTH_BYTES + 4U);

	return space > 1U;
}

bool CModem::writeDStarData(const unsigned char* data, unsigned int length)
{
	if (m_txInhibit)
		return true;

	assert(data != nullptr);
	assert(length > 0U);

	unsigned char buffer[50U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = length + 2U;

	switch (data[0U]) {
		case TAG_HEADER:
			buffer[2U] = MMDVM_DSTAR_HEADER;
			break;
		case TAG_DATA:
			buffer[2U] = MMDVM_DSTAR_DATA;
			break;
		case TAG_EOT:
			buffer[2U] = MMDVM_DSTAR_EOT;
			break;
		default:
			CUtils::dump(2U, "Unknown D-Star packet type", data, length);
			return false;
	}

	::memcpy(buffer + 3U, data + 1U, length - 1U);

	unsigned char len = length + 2U;
	m_txDStarData.addData(&len, 1U);
	m_txDStarData.addData(buffer, len);

	return true;
}
#endif

#if defined(USE_DMR)
bool CModem::hasDMRSpace1() const
{
	unsigned int space = m_txDMRData1.freeSpace() / (DMR_FRAME_LENGTH_BYTES + 4U);

	return space > 1U;
}

bool CModem::hasDMRSpace2() const
{
	unsigned int space = m_txDMRData2.freeSpace() / (DMR_FRAME_LENGTH_BYTES + 4U);

	return space > 1U;
}

bool CModem::writeDMRData1(const unsigned char* data, unsigned int length)
{
	if (m_txInhibit)
		return true;

	assert(data != nullptr);
	assert(length > 0U);

	if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
		return false;

	unsigned char buffer[40U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = length + 2U;
	buffer[2U] = MMDVM_DMR_DATA1;

	::memcpy(buffer + 3U, data + 1U, length - 1U);

	unsigned char len = length + 2U;
	m_txDMRData1.addData(&len, 1U);
	m_txDMRData1.addData(buffer, len);

	return true;
}

bool CModem::writeDMRData2(const unsigned char* data, unsigned int length)
{
	if (m_txInhibit)
		return true;

	assert(data != nullptr);
	assert(length > 0U);

	if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
		return false;

	unsigned char buffer[40U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = length + 2U;
	buffer[2U] = MMDVM_DMR_DATA2;

	::memcpy(buffer + 3U, data + 1U, length - 1U);

	unsigned char len = length + 2U;
	m_txDMRData2.addData(&len, 1U);
	m_txDMRData2.addData(buffer, len);

	return true;
}
#endif

#if defined(USE_YSF)
bool CModem::hasYSFSpace() const
{
	unsigned int space = m_txYSFData.freeSpace() / (YSF_FRAME_LENGTH_BYTES + 4U);

	return space > 1U;
}

bool CModem::writeYSFData(const unsigned char* data, unsigned int length)
{
	if (m_txInhibit)
		return true;

	assert(data != nullptr);
	assert(length > 0U);

	if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
		return false;

	unsigned char buffer[130U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = length + 2U;
	buffer[2U] = MMDVM_YSF_DATA;

	::memcpy(buffer + 3U, data + 1U, length - 1U);

	unsigned char len = length + 2U;
	m_txYSFData.addData(&len, 1U);
	m_txYSFData.addData(buffer, len);

	return true;
}
#endif

#if defined(USE_P25)
bool CModem::hasP25Space() const
{
	unsigned int space = m_txP25Data.freeSpace() / (P25_LDU_FRAME_LENGTH_BYTES + 4U);

	return space > 1U;
}

bool CModem::writeP25Data(const unsigned char* data, unsigned int length)
{
	if (m_txInhibit)
		return true;

	assert(data != nullptr);
	assert(length > 0U);

	if (data[0U] != TAG_HEADER && data[0U] != TAG_DATA && data[0U] != TAG_EOT)
		return false;

	unsigned char buffer[250U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = length + 2U;
	buffer[2U] = (data[0U] == TAG_HEADER) ? MMDVM_P25_HDR : MMDVM_P25_LDU;

	::memcpy(buffer + 3U, data + 1U, length - 1U);

	unsigned char len = length + 2U;
	m_txP25Data.addData(&len, 1U);
	m_txP25Data.addData(buffer, len);

	return true;
}
#endif

#if defined(USE_NXDN)
bool CModem::hasNXDNSpace() const
{
	unsigned int space = m_txNXDNData.freeSpace() / (NXDN_FRAME_LENGTH_BYTES + 4U);

	return space > 1U;
}

bool CModem::writeNXDNData(const unsigned char* data, unsigned int length)
{
	if (m_txInhibit)
		return true;

	assert(data != nullptr);
	assert(length > 0U);

	if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
		return false;

	unsigned char buffer[130U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = length + 2U;
	buffer[2U] = MMDVM_NXDN_DATA;

	::memcpy(buffer + 3U, data + 1U, length - 1U);

	unsigned char len = length + 2U;
	m_txNXDNData.addData(&len, 1U);
	m_txNXDNData.addData(buffer, len);

	return true;
}
#endif

#if defined(USE_POCSAG)
bool CModem::hasPOCSAGSpace() const
{
	unsigned int space = m_txPOCSAGData.freeSpace() / (POCSAG_FRAME_LENGTH_BYTES + 4U);

	return space > 1U;
}

bool CModem::writePOCSAGData(const unsigned char* data, unsigned int length)
{
	if (m_txInhibit)
		return true;

	assert(data != nullptr);
	assert(length > 0U);

	unsigned char buffer[130U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = length + 3U;
	buffer[2U] = MMDVM_POCSAG_DATA;

	::memcpy(buffer + 3U, data, length);

	unsigned char len = length + 3U;
	m_txPOCSAGData.addData(&len, 1U);
	m_txPOCSAGData.addData(buffer, len);

	return true;
}
#endif

#if defined(USE_FM)
unsigned int CModem::getFMSpace() const
{
	return m_txFMData.freeSpace();
}

bool CModem::writeFMData(const unsigned char* data, unsigned int length)
{
	if (m_txInhibit)
		return true;

	assert(data != nullptr);
	assert(length > 0U);

	unsigned char buffer[500U];

	unsigned int len;
	if (length > 252U) {
		buffer[0U] = MMDVM_FRAME_START;
		buffer[1U] = 0U;
		buffer[2U] = (length + 4U) - 255U;
		buffer[3U] = MMDVM_FM_DATA;
		::memcpy(buffer + 4U, data, length);
		len = length + 4U;
	} else {
		buffer[0U] = MMDVM_FRAME_START;
		buffer[1U] = length + 3U;
		buffer[2U] = MMDVM_FM_DATA;
		::memcpy(buffer + 3U, data, length);
		len = length + 3U;
	}

	m_txFMData.addData((unsigned char*)&len, sizeof(unsigned int));
	m_txFMData.addData(buffer, len);

	return true;
}
#endif

bool CModem::writeTransparentData(const unsigned char* data, unsigned int length)
{
	assert(data != nullptr);
	assert(length > 0U);

	unsigned char buffer[250U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = length + 3U;
	buffer[2U] = MMDVM_TRANSPARENT;

	if (m_sendTransparentDataFrameType > 0U) {
		::memcpy(buffer + 2U, data, length);
		length--;
		buffer[1U]--;

		//when sendFrameType==1 , only 0x80 and 0x90 (MMDVM_SERIAL and MMDVM_TRANSPARENT) are allowed
		//  and reverted to default (MMDVM_TRANSPARENT) for any other value
		//when >1, frame type is not checked
		if (m_sendTransparentDataFrameType == 1U) {
			if ((buffer[2U] & 0xE0) != 0x80)
				buffer[2U] = MMDVM_TRANSPARENT;
		}
	} else {
		::memcpy(buffer + 3U, data, length);
	}

	unsigned char len = length + 3U;
	m_txTransparentData.addData(&len, 1U);
	m_txTransparentData.addData(buffer, len);

	return true;
}

#if defined(USE_DSTAR)
bool CModem::writeDStarInfo(const char* my1, const char* my2, const char* your, const char* type, const char* reflector)
{
	assert(m_port != nullptr);
	assert(my1 != nullptr);
	assert(my2 != nullptr);
	assert(your != nullptr);
	assert(type != nullptr);
	assert(reflector != nullptr);

	unsigned char buffer[50U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = 33U;
	buffer[2U] = MMDVM_QSO_INFO;

	buffer[3U] = MODE_DSTAR;

	::memcpy(buffer + 4U,  my1,  DSTAR_LONG_CALLSIGN_LENGTH);
	::memcpy(buffer + 12U, my2,  DSTAR_SHORT_CALLSIGN_LENGTH);

	::memcpy(buffer + 16U, your, DSTAR_LONG_CALLSIGN_LENGTH);

	::memcpy(buffer + 24U, type, 1U);

	::memcpy(buffer + 25U, reflector, DSTAR_LONG_CALLSIGN_LENGTH);

	return m_port->write(buffer, 33U) != 33;
}
#endif

#if defined(USE_DMR)
bool CModem::writeDMRInfo(unsigned int slotNo, const std::string& src, bool group, const std::string& dest, const char* type)
{
	assert(m_port != nullptr);
	assert(type != nullptr);

	unsigned char buffer[50U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = 47U;
	buffer[2U] = MMDVM_QSO_INFO;

	buffer[3U] = MODE_DMR;

	buffer[4U] = slotNo;

	::sprintf((char*)(buffer + 5U), "%20.20s", src.c_str());

	buffer[25U] = group ? 'G' : 'I';

	::sprintf((char*)(buffer + 26U), "%20.20s", dest.c_str());

	::memcpy(buffer + 46U, type, 1U);

	return m_port->write(buffer, 47U) != 47;
}
#endif

#if defined(USE_YSF)
bool CModem::writeYSFInfo(const char* source, const char* dest, unsigned char dgid, const char* type, const char* origin)
{
	assert(m_port != nullptr);
	assert(source != nullptr);
	assert(dest != nullptr);
	assert(type != nullptr);
	assert(origin != nullptr);

	unsigned char buffer[40U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = 36U;
	buffer[2U] = MMDVM_QSO_INFO;

	buffer[3U] = MODE_YSF;

	::memcpy(buffer + 4U,  source, YSF_CALLSIGN_LENGTH);
	::memcpy(buffer + 14U, dest,   YSF_CALLSIGN_LENGTH);

	::memcpy(buffer + 24U, type, 1U);

	::memcpy(buffer + 25U, origin, YSF_CALLSIGN_LENGTH);

	buffer[35U] = dgid;

	return m_port->write(buffer, 36U) != 36;
}
#endif

#if defined(USE_P25)
bool CModem::writeP25Info(const char* source, bool group, unsigned int dest, const char* type)
{
	assert(m_port != nullptr);
	assert(source != nullptr);
	assert(type != nullptr);

	unsigned char buffer[40U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = 31U;
	buffer[2U] = MMDVM_QSO_INFO;

	buffer[3U] = MODE_P25;

	::sprintf((char*)(buffer + 4U), "%20.20s", source);

	buffer[24U] = group ? 'G' : 'I';

	::sprintf((char*)(buffer + 25U), "%05u", dest);	// 16-bits

	::memcpy(buffer + 30U, type, 1U);

	return m_port->write(buffer, 31U) != 31;
}
#endif

#if defined(USE_NXDN)
bool CModem::writeNXDNInfo(const char* source, bool group, unsigned int dest, const char* type)
{
	assert(m_port != nullptr);
	assert(source != nullptr);
	assert(type != nullptr);

	unsigned char buffer[40U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = 31U;
	buffer[2U] = MMDVM_QSO_INFO;

	buffer[3U] = MODE_NXDN;

	::sprintf((char*)(buffer + 4U), "%20.20s", source);

	buffer[24U] = group ? 'G' : 'I';

	::sprintf((char*)(buffer + 25U), "%05u", dest);	// 16-bits

	::memcpy(buffer + 30U, type, 1U);

	return m_port->write(buffer, 31U) != 31;
}
#endif

#if defined(USE_POCSAG)
bool CModem::writePOCSAGInfo(unsigned int ric, const std::string& message)
{
	assert(m_port != nullptr);

	size_t length = message.size();

	unsigned char buffer[250U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = (unsigned char)length + 11U;
	buffer[2U] = MMDVM_QSO_INFO;

	buffer[3U] = MODE_POCSAG;

	::sprintf((char*)(buffer + 4U), "%07u", ric);	// 21-bits

	::memcpy(buffer + 11U, message.c_str(), length);

	int ret = m_port->write(buffer, (unsigned int)length + 11U);

	return ret != int(length + 11U);
}
#endif

bool CModem::writeIPInfo(const std::string& address)
{
	assert(m_port != nullptr);

	size_t length = address.size();

	unsigned char buffer[60U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = (unsigned char)length + 4U;
	buffer[2U] = MMDVM_QSO_INFO;

	buffer[3U] = 250U;

	::memcpy(buffer + 4U, address.c_str(), length);

	int ret = m_port->write(buffer, (unsigned int)length + 4U);

	return ret != int(length + 4U);
}

bool CModem::writeSerialData(const unsigned char* data, unsigned int length)
{
	assert(m_port != nullptr);
	assert(data != nullptr);
	assert(length > 0U);

	unsigned char buffer[255U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = length + 3U;
	buffer[2U] = MMDVM_SERIAL_DATA;

	::memcpy(buffer + 3U, data, length);

	unsigned char len = length + 3U;
	m_txSerialData.addData(&len, 1U);
	m_txSerialData.addData(buffer, len);

	return true;
}

// A QSO information frame built by the caller (mode byte first), for a
// modem with its own screen such as the DVMega CAST. Queued with the
// serial data so it is written from the modem's own thread.
bool CModem::writeQSOInfo(const unsigned char* data, unsigned int length)
{
	assert(data != nullptr);

	if (length == 0U || length > 250U)
		return false;

	unsigned char buffer[255U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = length + 3U;
	buffer[2U] = MMDVM_QSO_INFO;

	::memcpy(buffer + 3U, data, length);

	unsigned char len = length + 3U;
	m_txSerialData.addData(&len, 1U);
	m_txSerialData.addData(buffer, len);

	return true;
}

bool CModem::hasTX() const
{
	return m_tx;
}

bool CModem::hasCD() const
{
	return m_cd;
}

bool CModem::hasLockout() const
{
	return m_lockout;
}

bool CModem::hasError() const
{
	return m_error;
}

bool CModem::hasDStar() const
{
	return (m_capabilities1 & CAP1_DSTAR) == CAP1_DSTAR;
}

bool CModem::hasDMR() const
{
	return (m_capabilities1 & CAP1_DMR) == CAP1_DMR;
}

bool CModem::hasYSF() const
{
	return (m_capabilities1 & CAP1_YSF) == CAP1_YSF;
}

bool CModem::hasP25() const
{
	return (m_capabilities1 & CAP1_P25) == CAP1_P25;
}

bool CModem::hasNXDN() const
{
	return (m_capabilities1 & CAP1_NXDN) == CAP1_NXDN;
}

bool CModem::hasFM() const
{
	return (m_capabilities1 & CAP1_FM) == CAP1_FM;
}

bool CModem::hasPOCSAG() const
{
	return (m_capabilities2 & CAP2_POCSAG) == CAP2_POCSAG;
}

unsigned int CModem::getVersion() const
{
	return m_protocolVersion;
}

bool CModem::readVersion()
{
	assert(m_port != nullptr);

	CThread::sleep(2000U);	// 2s

	for (unsigned int i = 0U; i < 6U; i++) {
		unsigned char buffer[3U];

		buffer[0U] = MMDVM_FRAME_START;
		buffer[1U] = 3U;
		buffer[2U] = MMDVM_GET_VERSION;

		// CUtils::dump(1U, "Written", buffer, 3U);

		int ret = m_port->write(buffer, 3U);
		if (ret != 3)
			return false;

#if defined(__APPLE__)
		m_port->setNonblock(true);
#endif

		for (unsigned int count = 0U; count < MAX_RESPONSES; count++) {
			CThread::sleep(10U);
			RESP_TYPE_MMDVM resp = getResponse();
			if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] == MMDVM_GET_VERSION)) {
				m_protocolVersion = m_buffer[3U];
				switch (m_protocolVersion) {
				case 1U:
					LogInfo("MMDVM protocol version: 1, description: %.*s", m_length - 4U, m_buffer + 4U);
					m_capabilities1 = CAP1_DSTAR | CAP1_DMR | CAP1_YSF | CAP1_P25 | CAP1_NXDN;
					m_capabilities2 = CAP2_POCSAG;
					break;

				case 2U:
					LogInfo("MMDVM protocol version: 2, description: %.*s", m_length - 23U, m_buffer + 23U);
					switch (m_buffer[6U]) {
					case 0U:
						LogInfo("CPU: Atmel ARM, UDID: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", m_buffer[7U], m_buffer[8U], m_buffer[9U], m_buffer[10U], m_buffer[11U], m_buffer[12U], m_buffer[13U], m_buffer[14U], m_buffer[15U], m_buffer[16U], m_buffer[17U], m_buffer[18U], m_buffer[19U], m_buffer[20U], m_buffer[21U], m_buffer[22U]);
						break;
					case 1U:
						LogInfo("CPU: NXP ARM, UDID: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", m_buffer[7U], m_buffer[8U], m_buffer[9U], m_buffer[10U], m_buffer[11U], m_buffer[12U], m_buffer[13U], m_buffer[14U], m_buffer[15U], m_buffer[16U], m_buffer[17U], m_buffer[18U], m_buffer[19U], m_buffer[20U], m_buffer[21U], m_buffer[22U]);
						break;
					case 2U:
						LogInfo("CPU: ST-Micro ARM, UDID: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", m_buffer[7U], m_buffer[8U], m_buffer[9U], m_buffer[10U], m_buffer[11U], m_buffer[12U], m_buffer[13U], m_buffer[14U], m_buffer[15U], m_buffer[16U], m_buffer[17U], m_buffer[18U]);
						break;
					default:
						LogInfo("CPU: Unknown type: %u", m_buffer[6U]);
						break;
					}
					m_capabilities1 = m_buffer[4U];
					m_capabilities2 = m_buffer[5U];
					break;

				default:
					LogError("MMDVM protocol version: %u, unsupported by this version of the MMDVM Host", m_protocolVersion);
					return false;
				}

				char modeText[100U];
				::strcpy(modeText, "Modes:");
				if (hasDStar())
					::strcat(modeText, " D-Star");
				if (hasDMR())
					::strcat(modeText, " DMR");
				if (hasYSF())
					::strcat(modeText, " YSF");
				if (hasP25())
					::strcat(modeText, " P25");
				if (hasNXDN())
					::strcat(modeText, " NXDN");
				if (hasFM())
					::strcat(modeText, " FM");
				if (hasPOCSAG())
					::strcat(modeText, " POCSAG");
				LogInfo(modeText);

				return true;
			}
		}

		CThread::sleep(1500U);
	}

	LogError("Unable to read the firmware version after six attempts");

	return false;
}

bool CModem::readStatus()
{
	assert(m_port != nullptr);

	unsigned char buffer[3U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = 3U;
	buffer[2U] = MMDVM_GET_STATUS;

	// CUtils::dump(1U, "Written", buffer, 3U);

	return m_port->write(buffer, 3U) == 3;
}

bool CModem::writeConfig()
{
	switch (m_protocolVersion) {
	case 1U:
		return setConfig1();
	case 2U:
		return setConfig2();
	default:
		return false;
	}
}

bool CModem::setConfig1()
{
	assert(m_port != nullptr);

	unsigned char buffer[30U];

	buffer[0U] = MMDVM_FRAME_START;

	buffer[1U] = 26U;

	buffer[2U] = MMDVM_SET_CONFIG;

	buffer[3U] = 0x00U;
	if (m_rxInvert)
		buffer[3U] |= 0x01U;
	if (m_txInvert)
		buffer[3U] |= 0x02U;
	if (m_pttInvert)
		buffer[3U] |= 0x04U;
#if defined(USE_YSF)
	if (m_ysfLoDev)
		buffer[3U] |= 0x08U;
#endif
	if (m_debug)
		buffer[3U] |= 0x10U;
	if (m_useCOSAsLockout)
		buffer[3U] |= 0x20U;
	if (!m_duplex)
		buffer[3U] |= 0x80U;

	buffer[4U] = 0x00U;
#if defined(USE_DSTAR)
	if (m_dstarEnabled)
		buffer[4U] |= 0x01U;
#endif
#if defined(USE_DMR)
	if (m_dmrEnabled)
		buffer[4U] |= 0x02U;
#endif
#if defined(USE_YSF)
	if (m_ysfEnabled)
		buffer[4U] |= 0x04U;
#endif
#if defined(USE_P25)
	if (m_p25Enabled)
		buffer[4U] |= 0x08U;
#endif
#if defined(USE_NXDN)
	if (m_nxdnEnabled)
		buffer[4U] |= 0x10U;
#endif
#if defined(USE_POCSAG)
	if (m_pocsagEnabled)
		buffer[4U] |= 0x20U;
#endif

	buffer[5U] = m_txDelay / 10U;		// In 10ms units

	buffer[6U] = MODE_IDLE;

	buffer[7U] = (unsigned char)(m_rxLevel * 2.55F + 0.5F);

	buffer[8U] = (unsigned char)(m_cwIdTXLevel * 2.55F + 0.5F);

#if defined(USE_DMR)
	buffer[9U]  = m_dmrColorCode;
	buffer[10U] = m_dmrDelay;
#else
	buffer[9U]  = 0U;
	buffer[10U] = 0U;
#endif

	buffer[11U] = 128U;           // Was OscOffset

#if defined(USE_DSTAR)
	buffer[12U] = (unsigned char)(m_dstarTXLevel * 2.55F + 0.5F);
#else
	buffer[12U] = 0U;
#endif
#if defined(USE_DMR)
	buffer[13U] = (unsigned char)(m_dmrTXLevel * 2.55F + 0.5F);
#else
	buffer[13U] = 0U;
#endif
#if defined(USE_YSF)
	buffer[14U] = (unsigned char)(m_ysfTXLevel * 2.55F + 0.5F);
#else
	buffer[14U] = 0U;
#endif
#if defined(USE_P25)
	buffer[15U] = (unsigned char)(m_p25TXLevel * 2.55F + 0.5F);
#else
	buffer[15U] = 0U;
#endif

	buffer[16U] = (unsigned char)(m_txDCOffset + 128);
	buffer[17U] = (unsigned char)(m_rxDCOffset + 128);

#if defined(USE_NXDN)
	buffer[18U] = (unsigned char)(m_nxdnTXLevel * 2.55F + 0.5F);
#else
	buffer[18U] = 0U;
#endif
#if defined(USE_YSF)
	buffer[19U] = (unsigned char)m_ysfTXHang;
#else
	buffer[19U] = 0U;
#endif
#if defined(USE_POCSAG)
	buffer[20U] = (unsigned char)(m_pocsagTXLevel * 2.55F + 0.5F);
#else
	buffer[20U] = 0U;
#endif
#if defined(USE_FM)
	buffer[21U] = (unsigned char)(m_fmTXLevel * 2.55F + 0.5F);
#else
	buffer[21U] = 0U;
#endif
#if defined(USE_P25)
	buffer[22U] = (unsigned char)m_p25TXHang;
#else
	buffer[22U] = 0U;
#endif
#if defined(USE_NXDN)
	buffer[23U] = (unsigned char)m_nxdnTXHang;
#else
	buffer[23U] = 0U;
#endif

	buffer[24U] = 0U;
	buffer[25U] = 0U;

	// CUtils::dump(1U, "Written", buffer, 26U);

	int ret = m_port->write(buffer, 26U);
	if (ret != 26)
		return false;

	unsigned int count = 0U;
	RESP_TYPE_MMDVM resp;
	do {
		CThread::sleep(10U);

		resp = getResponse();
		if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK)) {
			count++;
			if (count >= MAX_RESPONSES) {
				LogError("The MMDVM is not responding to the SET_CONFIG command");
				return false;
			}
		}
	} while ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK));

	// CUtils::dump(1U, "Response", m_buffer, m_length);

	if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] == MMDVM_NAK)) {
		LogError("Received a NAK to the SET_CONFIG command from the modem");
		return false;
	}

	m_playoutTimer.start();

	return true;
}

bool CModem::setConfig2()
{
	assert(m_port != nullptr);

	unsigned char buffer[50U];

	buffer[0U] = MMDVM_FRAME_START;

	buffer[1U] = 40U;

	buffer[2U] = MMDVM_SET_CONFIG;

	buffer[3U] = 0x00U;
	if (m_rxInvert)
		buffer[3U] |= 0x01U;
	if (m_txInvert)
		buffer[3U] |= 0x02U;
	if (m_pttInvert)
		buffer[3U] |= 0x04U;
#if defined(USE_YSF)
	if (m_ysfLoDev)
		buffer[3U] |= 0x08U;
#endif
	if (m_debug)
		buffer[3U] |= 0x10U;
	if (m_useCOSAsLockout)
		buffer[3U] |= 0x20U;
	if (!m_duplex)
		buffer[3U] |= 0x80U;

	buffer[4U] = 0x00U;
#if defined(USE_DSTAR)
	if (m_dstarEnabled)
		buffer[4U] |= 0x01U;
#endif
#if defined(USE_DMR)
	if (m_dmrEnabled)
		buffer[4U] |= 0x02U;
#endif
#if defined(USE_YSF)
	if (m_ysfEnabled)
		buffer[4U] |= 0x04U;
#endif
#if defined(USE_P25)
	if (m_p25Enabled)
		buffer[4U] |= 0x08U;
#endif
#if defined(USE_NXDN)
	if (m_nxdnEnabled)
		buffer[4U] |= 0x10U;
#endif
#if defined(USE_FM)
	if (m_fmEnabled)
		buffer[4U] |= 0x20U;
#endif

	buffer[5U] = 0x00U;
#if defined(USE_POCSAG)
	if (m_pocsagEnabled)
		buffer[5U] |= 0x01U;
#endif
	buffer[6U] = m_txDelay / 10U;		// In 10ms units

	buffer[7U] = MODE_IDLE;

	buffer[8U] = (unsigned char)(m_txDCOffset + 128);
	buffer[9U] = (unsigned char)(m_rxDCOffset + 128);

	buffer[10U] = (unsigned char)(m_rxLevel * 2.55F + 0.5F);

	buffer[11U] = (unsigned char)(m_cwIdTXLevel * 2.55F + 0.5F);

#if defined(USE_DSTAR)
	buffer[12U] = (unsigned char)(m_dstarTXLevel * 2.55F + 0.5F);
#else
	buffer[12U] = 0U;
#endif
#if defined(USE_DMR)
	buffer[13U] = (unsigned char)(m_dmrTXLevel * 2.55F + 0.5F);
#else
	buffer[13U] = 0U;
#endif
#if defined(USE_YSF)
	buffer[14U] = (unsigned char)(m_ysfTXLevel * 2.55F + 0.5F);
#else
	buffer[14U] = 0U;
#endif
#if defined(USE_P25)
	buffer[15U] = (unsigned char)(m_p25TXLevel * 2.55F + 0.5F);
#else
	buffer[15U] = 0U;
#endif
#if defined(USE_NXDN)
	buffer[16U] = (unsigned char)(m_nxdnTXLevel * 2.55F + 0.5F);
#else
	buffer[16U] = 0U;
#endif

	buffer[17U] = 0U;

#if defined(USE_POCSAG)
	buffer[18U] = (unsigned char)(m_pocsagTXLevel * 2.55F + 0.5F);
#else
	buffer[18U] = 0U;
#endif
#if defined(USE_FM)
	buffer[19U] = (unsigned char)(m_fmTXLevel * 2.55F + 0.5F);
#else
	buffer[19U] = 0U;
#endif

	buffer[20U] = 0U;
	buffer[21U] = 0x00U;
	buffer[22U] = 0x00U;

#if defined(USE_YSF)
	buffer[23U] = (unsigned char)m_ysfTXHang;
#else
	buffer[23U] = 0U;
#endif
#if defined(USE_P25)
	buffer[24U] = (unsigned char)m_p25TXHang;
#else
	buffer[24U] = 0U;
#endif
#if defined(USE_NXDN)
	buffer[25U] = (unsigned char)m_nxdnTXHang;
#else
	buffer[25U] = 0U;
#endif

	buffer[26U] = 0U;
	buffer[27U] = 0x00U;

#if defined(USE_DMR)
	buffer[28U] = m_dmrTrunkingEnabled ? 0x80U : 0x00U;
	buffer[29U] = m_dmrColorCode;
	buffer[30U] = m_dmrDelay;
#else
	buffer[28U] = 0U;
	buffer[29U] = 0U;
	buffer[30U] = 0U;
#endif

	buffer[31U] = 128U;
	buffer[32U] = 0U;
	buffer[33U] = 0U;
	buffer[34U] = 0U;

	buffer[35U] = 0x00U;
	buffer[36U] = 0x00U;
	buffer[37U] = 0x00U;
	buffer[38U] = 0x00U;
	buffer[39U] = 0x00U;

	// CUtils::dump(1U, "Written", buffer, 40U);

	int ret = m_port->write(buffer, 40U);
	if (ret != 40)
		return false;

	unsigned int count = 0U;
	RESP_TYPE_MMDVM resp;
	do {
		CThread::sleep(10U);

		resp = getResponse();
		if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK)) {
			count++;
			if (count >= MAX_RESPONSES) {
				LogError("The MMDVM is not responding to the SET_CONFIG command");
				return false;
			}
		}
	} while ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK));

	// CUtils::dump(1U, "Response", m_buffer, m_length);

	if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] == MMDVM_NAK)) {
		LogError("Received a NAK to the SET_CONFIG command from the modem");
		return false;
	}

	m_playoutTimer.start();

	return true;
}

bool CModem::setFrequency()
{
	assert(m_port != nullptr);

	unsigned char buffer[20U];

#if defined(USE_POCSAG)
	unsigned int  pocsagFrequency = 433000000U;
	if (m_pocsagEnabled)
		pocsagFrequency = m_pocsagFrequency;
#endif

	buffer[12U]  = (unsigned char)(m_rfLevel * 2.55F + 0.5F);

#if defined(USE_POCSAG)
	buffer[13U] = (pocsagFrequency >> 0)  & 0xFFU;
	buffer[14U] = (pocsagFrequency >> 8)  & 0xFFU;
	buffer[15U] = (pocsagFrequency >> 16) & 0xFFU;
	buffer[16U] = (pocsagFrequency >> 24) & 0xFFU;
#else
	buffer[13U] = 0U;
	buffer[14U] = 0U;
	buffer[15U] = 0U;
	buffer[16U] = 0U;
#endif

	buffer[0U]  = MMDVM_FRAME_START;

	buffer[1U]  = 17U;

	buffer[2U]  = MMDVM_SET_FREQ;

	buffer[3U]  = 0x00U;

	buffer[4U]  = (m_rxFrequency >> 0) & 0xFFU;
	buffer[5U]  = (m_rxFrequency >> 8) & 0xFFU;
	buffer[6U]  = (m_rxFrequency >> 16) & 0xFFU;
	buffer[7U]  = (m_rxFrequency >> 24) & 0xFFU;

	buffer[8U]  = (m_txFrequency >> 0) & 0xFFU;
	buffer[9U]  = (m_txFrequency >> 8) & 0xFFU;
	buffer[10U] = (m_txFrequency >> 16) & 0xFFU;
	buffer[11U] = (m_txFrequency >> 24) & 0xFFU;

	// CUtils::dump(1U, "Written", buffer, 17U);

	int ret = m_port->write(buffer, 17U);
	if (ret != 17)
		return false;

	unsigned int count = 0U;
	RESP_TYPE_MMDVM resp;
	do {
		CThread::sleep(10U);

		resp = getResponse();
		if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK)) {
			count++;
			if (count >= MAX_RESPONSES) {
				LogError("The MMDVM is not responding to the SET_FREQ command");
				return false;
			}
		}
	} while ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK));

	// CUtils::dump(1U, "Response", m_buffer, m_length);

	if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] == MMDVM_NAK)) {
		LogError("Received a NAK to the SET_FREQ command from the modem");
		return false;
	}

	return true;
}

RESP_TYPE_MMDVM CModem::getResponse()
{
	assert(m_port != nullptr);

	if (m_state == SERIAL_STATE::START) {
		// Get the start of the frame or nothing at all
		int ret = m_port->read(m_buffer + 0U, 1U);
		if (ret < 0) {
			LogError("Error when reading from the modem");
			return RESP_TYPE_MMDVM::ERR;
		}

		if (ret == 0)
			return RESP_TYPE_MMDVM::TIMEOUT;

		if (m_buffer[0U] != MMDVM_FRAME_START)
			return RESP_TYPE_MMDVM::TIMEOUT;

		m_state  = SERIAL_STATE::LENGTH1;
		m_length = 1U;
	}

	if (m_state == SERIAL_STATE::LENGTH1) {
		// Get the length of the frame, 1/2
		int ret = m_port->read(m_buffer + 1U, 1U);
		if (ret < 0) {
			LogError("Error when reading from the modem");
			m_state = SERIAL_STATE::START;
			return RESP_TYPE_MMDVM::ERR;
		}

		if (ret == 0)
			return RESP_TYPE_MMDVM::TIMEOUT;

		m_length = m_buffer[1U];
		m_offset = 2U;

		if (m_length == 0U)
			m_state = SERIAL_STATE::LENGTH2;
		else
			m_state = SERIAL_STATE::TYPE;
	}

	if (m_state == SERIAL_STATE::LENGTH2) {
		// Get the length of the frane, 2/2
		int ret = m_port->read(m_buffer + 2U, 1U);
		if (ret < 0) {
			LogError("Error when reading from the modem");
			m_state = SERIAL_STATE::START;
			return RESP_TYPE_MMDVM::ERR;
		}

		if (ret == 0)
			return RESP_TYPE_MMDVM::TIMEOUT;

		m_length = m_buffer[2U] + 255U;
		m_offset = 3U;
		m_state  = SERIAL_STATE::TYPE;
	}

	if (m_state == SERIAL_STATE::TYPE) {
		// Get the frame type
		int ret = m_port->read(&m_type, 1U);
		if (ret < 0) {
			LogError("Error when reading from the modem");
			m_state = SERIAL_STATE::START;
			return RESP_TYPE_MMDVM::ERR;
		}

		if (ret == 0)
			return RESP_TYPE_MMDVM::TIMEOUT;

		m_buffer[m_offset++] = m_type;

		m_state = SERIAL_STATE::DATA;
	}

	if (m_state == SERIAL_STATE::DATA) {
		while (m_offset < m_length) {
			int ret = m_port->read(m_buffer + m_offset, m_length - m_offset);
			if (ret < 0) {
				LogError("Error when reading from the modem");
				m_state = SERIAL_STATE::START;
				return RESP_TYPE_MMDVM::ERR;
			}

			if (ret == 0)
				return RESP_TYPE_MMDVM::TIMEOUT;

			if (ret > 0)
				m_offset += ret;
		}
	}

	// CUtils::dump(1U, "Received", m_buffer, m_length);

	m_offset = m_length > 255U ? 4U : 3U;
	m_state  = SERIAL_STATE::START;

	return RESP_TYPE_MMDVM::OK;
}

unsigned char CModem::getMode() const
{
	return m_mode;
}

bool CModem::setMode(unsigned char mode)
{
	assert(m_port != nullptr);

	unsigned char buffer[4U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = 4U;
	buffer[2U] = MMDVM_SET_MODE;
	buffer[3U] = mode;

	// CUtils::dump(1U, "Written", buffer, 4U);

	return m_port->write(buffer, 4U) == 4;
}

bool CModem::sendCWId(const std::string& callsign)
{
	if (m_txInhibit)
		return true;

	assert(m_port != nullptr);

	unsigned int length = (unsigned int)callsign.length();
	if (length > 200U)
		length = 200U;

	unsigned char buffer[205U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = length + 3U;
	buffer[2U] = MMDVM_SEND_CWID;

	for (unsigned int i = 0U; i < length; i++)
		buffer[i + 3U] = callsign.at(i);

	// CUtils::dump(1U, "Written", buffer, length + 3U);

	return m_port->write(buffer, length + 3U) == int(length + 3U);
}

#if defined(USE_DMR)
bool CModem::writeDMRStart(bool tx)
{
	assert(m_port != nullptr);

	// Listen-only: never key up.
	if (m_txInhibit)
		tx = false;

	if (tx && m_tx)
		return true;
	if (!tx && !m_tx)
		return true;

	unsigned char buffer[4U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = 4U;
	buffer[2U] = MMDVM_DMR_START;
	buffer[3U] = tx ? 0x01U : 0x00U;

	// CUtils::dump(1U, "Written", buffer, 4U);

	return m_port->write(buffer, 4U) == 4;
}

bool CModem::writeDMRAbort(unsigned int slotNo)
{
	assert(m_port != nullptr);

	if (slotNo == 1U)
		m_txDMRData1.clear();
	else
		m_txDMRData2.clear();

	unsigned char buffer[4U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = 4U;
	buffer[2U] = MMDVM_DMR_ABORT;
	buffer[3U] = slotNo;

	// CUtils::dump(1U, "Written", buffer, 4U);

	return m_port->write(buffer, 4U) == 4;
}

bool CModem::writeDMRAloha(unsigned int systemCode, bool registrationRequired, bool alternateSlot)
{
	if (m_txInhibit)
		return true;

	// 7.1.1.1.4 Aloha (C_ALOHA) CSBK PDU
	assert(m_port != NULL);
	CDMRCSBK csbk;
	unsigned char data[DMR_FRAME_LENGTH_BYTES + 2U];
	unsigned int tsccas = alternateSlot ? 1 : 0; 
	unsigned int timeslot_synchronization = 0;
	unsigned int version = 2; // 3 bits
	unsigned int offset_timing = 0; // MMDVM uses aligned timing
	unsigned int net_connection = 1;
	unsigned int mask = 0; // 5 bits
	unsigned int service_function = 0; // 2 bits (ALL)
	unsigned int n_rand_wait = 5; // 4 bits
	unsigned int backoff = 4; // 4 bits
	unsigned int ALLMSI = 0xFFFED4;
	
	data[2U] = 0x99;
	data[3U] = 0x00; // FID
	data[4U] = (unsigned char)((tsccas << 6) | (timeslot_synchronization << 5) | ((version & 0x07) << 2) | (offset_timing << 1) | net_connection);
	data[5U] = (unsigned char)(((mask & 0x1F)  << 3) | ((service_function & 0x03) << 1) | ((n_rand_wait & 0x0F) >> 3)); // TODO
	data[6U] = (unsigned char)(((n_rand_wait & 0x0F) << 5) | (((unsigned int)registrationRequired) << 4) | (backoff & 0xF)); // TODO
	data[7U] = (unsigned char)((systemCode >> 6) & 0xFF);
	data[8U] = (unsigned char)(((systemCode << 2) | 0x03) & 0xFF);  // PAR hardcoded to 11
	// MS Address
	data[9U] = (unsigned char)(ALLMSI >> 16);
	data[10U] = (unsigned char)((ALLMSI >> 8) & 0xFF);
	data[11U] = (unsigned char)(ALLMSI & 0xFF);
	csbk.setCSBKData(data + 2U);
	csbk.get(data + 2U);

	CDMRSlotType slotType;
	slotType.setColorCode(m_dmrColorCode);
	slotType.setDataType(DT_CSBK);
	slotType.getData(data + 2U);

	CSync::addDMRDataSync(data + 2U, m_duplex);
	unsigned char buffer[DMR_FRAME_LENGTH_BYTES + 3U];

	buffer[0U]  = MMDVM_FRAME_START;
	buffer[1U]  = DMR_FRAME_LENGTH_BYTES + 3U;
	buffer[2U]  = MMDVM_DMR_ALOHA;
	for(unsigned int i=0; i<DMR_FRAME_LENGTH_BYTES; i++)
	{
		buffer[i + 3U]  = data[i+ 2U];
	}

	return m_port->write(buffer, DMR_FRAME_LENGTH_BYTES + 3U) == 12;
}

void CModem::setDMRShortLC(unsigned int systemCode, bool isControlChannel, bool registrationRequired)
{
	// Used for setting CACH (C_SysParm and P_SysParm) on TSCC and Payload Ch
	unsigned char lc[5U];
	// 7.1.2.1 Control Channel System Parameters
	// 7.1.2.2 Payload Channel System Parameters
	lc[0U] = isControlChannel? 0x2U : 0x3U;
	lc[1U] = (unsigned char)((systemCode >> 6) & 0XFF);
	unsigned int regi = (unsigned int) registrationRequired;
	lc[2U] = (unsigned char)(((systemCode << 2) & 0xFF) | (regi << 1)); // 6 bit System code, 1 bit Reg_Required, rest of bits CSC
	lc[3U] = 0x01U; // CSC: This should increment from 1 to 511, but not implemented yet
	lc[4U] = CCRC::crc8(lc, 4U);
	unsigned char sLC[9U];
	CDMRShortLC shortLC;
	shortLC.encode(lc, sLC);
	this->writeDMRShortLC(sLC, true);
}

bool CModem::writeDMRShortLC(const unsigned char* lc, bool control)
{
	if (m_txInhibit)
		return true;

	if (!control && m_dmrTrunkingEnabled)
		return true;

	assert(m_port != nullptr);
	assert(lc != nullptr);

	unsigned char buffer[12U];

	buffer[0U]  = MMDVM_FRAME_START;
	buffer[1U]  = 12U;
	buffer[2U]  = MMDVM_DMR_SHORTLC;
	buffer[3U]  = lc[0U];
	buffer[4U]  = lc[1U];
	buffer[5U]  = lc[2U];
	buffer[6U]  = lc[3U];
	buffer[7U]  = lc[4U];
	buffer[8U]  = lc[5U];
	buffer[9U]  = lc[6U];
	buffer[10U] = lc[7U];
	buffer[11U] = lc[8U];

	// CUtils::dump(1U, "Written", buffer, 12U);

	return m_port->write(buffer, 12U) == 12;
}
#endif

#if defined(USE_FM)
void CModem::setFMCallsignParams(const std::string& callsign, unsigned int callsignSpeed, unsigned int callsignFrequency, unsigned int callsignTime, unsigned int callsignHoldoff, float callsignHighLevel, float callsignLowLevel, bool callsignAtStart, bool callsignAtEnd, bool callsignAtLatch)
{
	m_fmCallsign          = callsign;
	m_fmCallsignSpeed     = callsignSpeed;
	m_fmCallsignFrequency = callsignFrequency;
	m_fmCallsignTime      = callsignTime;
	m_fmCallsignHoldoff   = callsignHoldoff;
	m_fmCallsignHighLevel = callsignHighLevel;
	m_fmCallsignLowLevel  = callsignLowLevel;
	m_fmCallsignAtStart   = callsignAtStart;
	m_fmCallsignAtEnd     = callsignAtEnd;
	m_fmCallsignAtLatch   = callsignAtLatch;
}

void CModem::setFMAckParams(const std::string& rfAck, unsigned int ackSpeed, unsigned int ackFrequency, unsigned int ackMinTime, unsigned int ackDelay, float ackLevel)
{
	m_fmRfAck        = rfAck;
	m_fmAckSpeed     = ackSpeed;
	m_fmAckFrequency = ackFrequency;
	m_fmAckMinTime   = ackMinTime;
	m_fmAckDelay     = ackDelay;
	m_fmAckLevel     = ackLevel;
}

void CModem::setFMMiscParams(unsigned int timeout, float timeoutLevel, float ctcssFrequency, unsigned int ctcssHighThreshold, unsigned int ctcssLowThreshold, float ctcssLevel, unsigned int kerchunkTime, unsigned int hangTime, unsigned int accessMode, bool linkMode, bool cosInvert, bool noiseSquelch, unsigned int squelchHighThreshold, unsigned int squelchLowThreshold, unsigned int rfAudioBoost, float maxDevLevel)
{
	m_fmTimeout      = timeout;
	m_fmTimeoutLevel = timeoutLevel;

	m_fmCtcssFrequency     = ctcssFrequency;
	m_fmCtcssHighThreshold = ctcssHighThreshold;
	m_fmCtcssLowThreshold  = ctcssLowThreshold;
	m_fmCtcssLevel         = ctcssLevel;

	m_fmKerchunkTime = kerchunkTime;

	m_fmHangTime     = hangTime;

	m_fmAccessMode = accessMode;
	m_fmLinkMode   = linkMode;
	m_fmCOSInvert  = cosInvert;

	m_fmNoiseSquelch         = noiseSquelch;
	m_fmSquelchHighThreshold = squelchHighThreshold;
	m_fmSquelchLowThreshold  = squelchLowThreshold;

	m_fmRFAudioBoost = rfAudioBoost;
	m_fmMaxDevLevel  = maxDevLevel;
}

void CModem::setFMExtParams(const std::string& ack, unsigned int audioBoost)
{
	m_fmExtAck        = ack;
	m_fmExtAudioBoost = audioBoost;
	m_fmExtEnable     = true;
}

bool CModem::setFMCallsignParams()
{
	assert(m_port != nullptr);

	unsigned char buffer[80U];
	unsigned char len = 10U + (unsigned char)m_fmCallsign.size();

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = len;
	buffer[2U] = MMDVM_FM_PARAMS1;

	buffer[3U] = m_fmCallsignSpeed;
	buffer[4U] = m_fmCallsignFrequency / 10U;
	buffer[5U] = m_fmCallsignTime;
	buffer[6U] = m_fmCallsignHoldoff;

	buffer[7U] = (unsigned char)(m_fmCallsignHighLevel * 2.55F + 0.5F);
	buffer[8U] = (unsigned char)(m_fmCallsignLowLevel * 2.55F + 0.5F);

	buffer[9U] = 0x00U;
	if (m_fmCallsignAtStart)
		buffer[9U] |= 0x01U;
	if (m_fmCallsignAtEnd)
		buffer[9U] |= 0x02U;
	if (m_fmCallsignAtLatch)
		buffer[9U] |= 0x04U;

	for (unsigned int i = 0U; i < m_fmCallsign.size(); i++)
		buffer[10U + i] = m_fmCallsign.at(i);

	// CUtils::dump(1U, "Written", buffer, len);

	int ret = m_port->write(buffer, len);
	if (ret != len)
		return false;

	unsigned int count = 0U;
	RESP_TYPE_MMDVM resp;
	do {
		CThread::sleep(10U);

		resp = getResponse();
		if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK)) {
			count++;
			if (count >= MAX_RESPONSES) {
				LogError("The MMDVM is not responding to the SET_FM_PARAMS1 command");
				return false;
			}
		}
	} while ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK));

	// CUtils::dump(1U, "Response", m_buffer, m_length);

	if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] == MMDVM_NAK)) {
		LogError("Received a NAK to the SET_FM_PARAMS1 command from the modem");
		return false;
	}

	return true;
}

bool CModem::setFMAckParams()
{
	assert(m_port != nullptr);

	unsigned char buffer[80U];
	unsigned char len = 8U + (unsigned char)m_fmRfAck.size();

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = len;
	buffer[2U] = MMDVM_FM_PARAMS2;

	buffer[3U] = m_fmAckSpeed;
	buffer[4U] = m_fmAckFrequency / 10U;
	buffer[5U] = m_fmAckMinTime;
	buffer[6U] = m_fmAckDelay / 10U;

	buffer[7U] = (unsigned char)(m_fmAckLevel * 2.55F + 0.5F);

	for (unsigned int i = 0U; i < m_fmRfAck.size(); i++)
		buffer[8U + i] = m_fmRfAck.at(i);

	// CUtils::dump(1U, "Written", buffer, len);

	int ret = m_port->write(buffer, len);
	if (ret != len)
		return false;

	unsigned int count = 0U;
	RESP_TYPE_MMDVM resp;
	do {
		CThread::sleep(10U);

		resp = getResponse();
		if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK)) {
			count++;
			if (count >= MAX_RESPONSES) {
				LogError("The MMDVM is not responding to the SET_FM_PARAMS2 command");
				return false;
			}
		}
	} while ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK));

	// CUtils::dump(1U, "Response", m_buffer, m_length);

	if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] == MMDVM_NAK)) {
		LogError("Received a NAK to the SET_FM_PARAMS2 command from the modem");
		return false;
	}

	return true;
}

bool CModem::setFMMiscParams()
{
	assert(m_port != nullptr);

	unsigned char buffer[20U];

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = 17U;
	buffer[2U] = MMDVM_FM_PARAMS3;

	buffer[3U] = m_fmTimeout / 5U;
	buffer[4U] = (unsigned char)(m_fmTimeoutLevel * 2.55F + 0.5F);

	buffer[5U] = (unsigned char)m_fmCtcssFrequency;
	buffer[6U] = m_fmCtcssHighThreshold;
	buffer[7U] = m_fmCtcssLowThreshold;
	buffer[8U] = (unsigned char)(m_fmCtcssLevel * 2.55F + 0.5F);

	buffer[9U]  = m_fmKerchunkTime;
	buffer[10U] = m_fmHangTime;

	buffer[11U] = m_fmAccessMode & 0x0FU;
	if (m_fmLinkMode)
		buffer[11U] |= 0x20U;
	if (m_fmNoiseSquelch)
		buffer[11U] |= 0x40U;
	if (m_fmCOSInvert)
		buffer[11U] |= 0x80U;

	buffer[12U] = m_fmRFAudioBoost;

	buffer[13U] = (unsigned char)(m_fmMaxDevLevel * 2.55F + 0.5F);

	buffer[14U] = (unsigned char)(m_rxLevel * 2.55F + 0.5F);

	buffer[15U] = m_fmSquelchHighThreshold;
	buffer[16U] = m_fmSquelchLowThreshold;

	// CUtils::dump(1U, "Written", buffer, 17U);

	int ret = m_port->write(buffer, 17U);
	if (ret != 17)
		return false;

	unsigned int count = 0U;
	RESP_TYPE_MMDVM resp;
	do {
		CThread::sleep(10U);

		resp = getResponse();
		if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK)) {
			count++;
			if (count >= MAX_RESPONSES) {
				LogError("The MMDVM is not responding to the SET_FM_PARAMS3 command");
				return false;
			}
		}
	} while ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK));

	// CUtils::dump(1U, "Response", m_buffer, m_length);

	if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] == MMDVM_NAK)) {
		LogError("Received a NAK to the SET_FM_PARAMS3 command from the modem");
		return false;
	}

	return true;
}

bool CModem::setFMExtParams()
{
	assert(m_port != nullptr);

	unsigned char buffer[80U];
	unsigned char len = 7U + (unsigned char)m_fmExtAck.size();

	buffer[0U] = MMDVM_FRAME_START;
	buffer[1U] = len;
	buffer[2U] = MMDVM_FM_PARAMS4;

	buffer[3U] = m_fmExtAudioBoost;
	buffer[4U] = m_fmAckSpeed;
	buffer[5U] = m_fmAckFrequency / 10U;

	buffer[6U] = (unsigned char)(m_fmAckLevel * 2.55F + 0.5F);

	for (unsigned int i = 0U; i < m_fmExtAck.size(); i++)
		buffer[7U + i] = m_fmExtAck.at(i);

	// CUtils::dump(1U, "Written", buffer, len);

	int ret = m_port->write(buffer, len);
	if (ret != len)
		return false;

	unsigned int count = 0U;
	RESP_TYPE_MMDVM resp;
	do {
		CThread::sleep(10U);

		resp = getResponse();
		if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK)) {
			count++;
			if (count >= MAX_RESPONSES) {
				LogError("The MMDVM is not responding to the SET_FM_PARAMS4 command");
				return false;
			}
		}
	} while ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] != MMDVM_ACK) && (m_buffer[2U] != MMDVM_NAK));

	// CUtils::dump(1U, "Response", m_buffer, m_length);

	if ((resp == RESP_TYPE_MMDVM::OK) && (m_buffer[2U] == MMDVM_NAK)) {
		LogError("Received a NAK to the SET_FM_PARAMS4 command from the modem");
		return false;
	}

	return true;
}
#endif

void CModem::printDebug()
{
	if (m_type == MMDVM_DEBUG1) {
		LogMessage("Debug: %.*s", m_length - m_offset - 0U, m_buffer + m_offset);
	} else if (m_type == MMDVM_DEBUG2) {
		short val1 = (m_buffer[m_length - 2U] << 8) | m_buffer[m_length - 1U];
		LogMessage("Debug: %.*s %d", m_length - m_offset - 2U, m_buffer + m_offset, val1);
	} else if (m_type == MMDVM_DEBUG3) {
		short val1 = (m_buffer[m_length - 4U] << 8) | m_buffer[m_length - 3U];
		short val2 = (m_buffer[m_length - 2U] << 8) | m_buffer[m_length - 1U];
		LogMessage("Debug: %.*s %d %d", m_length - m_offset - 4U, m_buffer + m_offset, val1, val2);
	} else if (m_type == MMDVM_DEBUG4) {
		short val1 = (m_buffer[m_length - 6U] << 8) | m_buffer[m_length - 5U];
		short val2 = (m_buffer[m_length - 4U] << 8) | m_buffer[m_length - 3U];
		short val3 = (m_buffer[m_length - 2U] << 8) | m_buffer[m_length - 1U];
		LogMessage("Debug: %.*s %d %d %d", m_length - m_offset - 6U, m_buffer + m_offset, val1, val2, val3);
	} else if (m_type == MMDVM_DEBUG5) {
		short val1 = (m_buffer[m_length - 8U] << 8) | m_buffer[m_length - 7U];
		short val2 = (m_buffer[m_length - 6U] << 8) | m_buffer[m_length - 5U];
		short val3 = (m_buffer[m_length - 4U] << 8) | m_buffer[m_length - 3U];
		short val4 = (m_buffer[m_length - 2U] << 8) | m_buffer[m_length - 1U];
		LogMessage("Debug: %.*s %d %d %d %d", m_length - m_offset - 8U, m_buffer + m_offset, val1, val2, val3, val4);
	} else if (m_type == MMDVM_DEBUG_DUMP) {
		CUtils::dump(1U, "Debug: Data", m_buffer + m_offset, m_length - m_offset);
	}
}

#if defined(USE_DMR)
bool CModem::getDMRTrunkingEnabled() const
{
	return m_dmrTrunkingEnabled;
}

bool CModem::getDMRControlChannel() const
{
	return m_dmrControlChannel;
}
#endif

void CModem::setTXInhibit(bool inhibit)
{
	m_txInhibit = inhibit;

	if (inhibit) {
		writeDMRStart(false);
		m_txDStarData.clear();
		m_txDMRData1.clear();
		m_txDMRData2.clear();
		m_txYSFData.clear();
		m_txP25Data.clear();
		m_txNXDNData.clear();
		m_txPOCSAGData.clear();
		m_txFMData.clear();
	}
}
