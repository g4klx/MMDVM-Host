/*
 *   Copyright (C) 2020,2021,2023,2025 by Jonathan Naylor G4KLX
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

#include "FMControl.h"
#include "Utils.h"

#if defined(USE_FM)

#include <string>

#define SWAP_BYTES_16(a) (((a >> 8) & 0x00FFU) | ((a << 8) & 0xFF00U))

const float        DEEMPHASIS_GAIN_DB  = 8.0F;  // Audio gain  adjustment
const float        PREEMPHASIS_GAIN_DB = 0.0F;  // Audio gain  adjustment
const float        FILTER_GAIN_DB      = 2.0F;  // Audio gain  adjustment
const unsigned int FM_MASK             = 0x00000FFFU;

const unsigned char FS_LISTENING         = 0U;
const unsigned char FS_KERCHUNK_RF       = 1U;
const unsigned char FS_RELAYING_RF       = 2U;
const unsigned char FS_RELAYING_WAIT_RF  = 3U;
const unsigned char FS_TIMEOUT_RF        = 4U;
const unsigned char FS_TIMEOUT_WAIT_RF   = 5U;
const unsigned char FS_KERCHUNK_EXT      = 6U;
const unsigned char FS_RELAYING_EXT      = 7U;
const unsigned char FS_RELAYING_WAIT_EXT = 8U;
const unsigned char FS_TIMEOUT_EXT       = 9U;
const unsigned char FS_TIMEOUT_WAIT_EXT  = 10U;
const unsigned char FS_HANG              = 11U;

CFMControl::CFMControl(CFMNetwork* network, float txAudioGain, float rxAudioGain, bool preEmphasisOn, bool deEmphasisOn, CRSSIInterpolator* rssiMapper) :
m_network(network),
m_txAudioGain(txAudioGain),
m_rxAudioGain(rxAudioGain),
m_preEmphasisOn(preEmphasisOn),
m_deEmphasisOn(deEmphasisOn),
m_rssiMapper(rssiMapper),
m_enabled(false),
m_incomingRFAudio(1600U, "Incoming RF FM Audio"),
m_preEmphasis(nullptr),
m_deEmphasis(nullptr),
m_filterStage1(nullptr),
m_filterStage2(nullptr),
m_filterStage3(nullptr)
{
	assert(txAudioGain > 0.0F);
	assert(rxAudioGain > 0.0F);
	assert(rssiMapper != nullptr);

	m_preEmphasis  = new CIIRDirectForm1Filter(8.315375384336983F, -7.03334621603483F,0.0F,1.0F, 0.282029168302153F,0.0F, PREEMPHASIS_GAIN_DB);
	m_deEmphasis   = new CIIRDirectForm1Filter(0.07708787090460224F, 0.07708787090460224F,0.0F, 1.0F, -0.8458242581907955F,0.0F, DEEMPHASIS_GAIN_DB);

	// Chebyshev type 1 0.2dB cheby type 1 3rd order 300-2700Hz fs=8000
	m_filterStage1 = new CIIRDirectForm1Filter(0.29495028f, 0.0f, -0.29495028f, 1.0f, -0.61384624f, -0.057158668f, FILTER_GAIN_DB);
	m_filterStage2 = new CIIRDirectForm1Filter(1.0f, 2.0f, 1.0f, 1.0f, 0.9946123f, 0.6050482f, FILTER_GAIN_DB);
	m_filterStage3 = new CIIRDirectForm1Filter(1.0f, -2.0f, 1.0f, 1.0f, -1.8414584f, 0.8804949f, FILTER_GAIN_DB);
}

CFMControl::~CFMControl()
{
	delete m_preEmphasis;
	delete m_deEmphasis;

	delete m_filterStage1;
	delete m_filterStage2;
	delete m_filterStage3;
}

bool CFMControl::writeModem(const unsigned char* data, unsigned int length)
{
    assert(data != nullptr);
    assert(length > 0U);

	if (data[0U] == TAG_HEADER) {
		switch (data[1U]) {
			case FS_LISTENING:         writeJSON("listening");         break;
			case FS_KERCHUNK_RF:       writeJSON("kerchunk_rf");       break;
			case FS_RELAYING_RF:       writeJSON("relaying_rf");       break;
			case FS_RELAYING_WAIT_RF:  writeJSON("relaying_wait_rf");  break;
			case FS_TIMEOUT_RF:        writeJSON("timeout_rf");        break;
			case FS_TIMEOUT_WAIT_RF:   writeJSON("timeout_wait_rf");   break;
			case FS_KERCHUNK_EXT:      writeJSON("kerchunk_ext");      break;
			case FS_RELAYING_EXT:      writeJSON("relaying_ext");      break;
			case FS_RELAYING_WAIT_EXT: writeJSON("relaying_wait_ext"); break;
			case FS_TIMEOUT_EXT:       writeJSON("timeout_ext");       break;
			case FS_TIMEOUT_WAIT_EXT:  writeJSON("timeout_wait_ext");  break;
			case FS_HANG:              writeJSON("hang");              break;
			default:                   writeJSON("unknown");           break;
		}

		return true;
	}

	if (data[0U] == TAG_RSSI) {
		uint16_t raw = 0U;
		raw |= (data[1U] << 8) & 0xFF00U;
		raw |= (data[2U] << 0) & 0x00FFU;

		// Convert the raw RSSI to dBm
		int rssi = m_rssiMapper->interpolate(raw);
		if (rssi != 0) {
			LogDebug("FM, raw RSSI: %u, reported RSSI: %d dBm", raw, rssi);
			writeJSONRSSI(rssi);
		}

		return true;
	}

	if (m_network == nullptr)
		return true;

	if (data[0U] == TAG_EOT) {
		// Reset the RX filter states at the end of each transmission so that
		// leftover IIR state cannot bleed into the next conversation.
		m_deEmphasis->reset();
		m_filterStage1->reset();
		m_filterStage2->reset();
		m_filterStage3->reset();

		return m_network->writeEnd();
	}

	if (data[0U] != TAG_DATA)
		return false;

	m_incomingRFAudio.addData(data + 1U, length - 1U);
	unsigned int bufferLength = m_incomingRFAudio.dataSize();

	if (bufferLength > 240U)		// 160 samples 12-bit
		bufferLength = 240U;		// 160 samples 12-bit

	if (bufferLength >= 3U) {
		bufferLength = bufferLength - bufferLength % 3U;	// Round down to nearest multiple of 3

		unsigned char bufferData[240U];		// 160 samples 12-bit       
		m_incomingRFAudio.getData(bufferData, bufferLength);

		unsigned int pack = 0U;
		unsigned char* packPointer = (unsigned char*)&pack;

		float out[160U];			// 160 samples 12-bit
		unsigned int nOut = 0U;

		for (unsigned int i = 0U; i < bufferLength; i += 3U) {
			// Extract unsigned 12 bit unsigned sample pairs packed into 3 bytes to 16 bit signed
			packPointer[0U] = bufferData[i + 0U];
			packPointer[1U] = bufferData[i + 1U];
			packPointer[2U] = bufferData[i + 2U];

			short unpackedSamples[2U];
			unpackedSamples[1U] = short(int((pack >> 0)  & FM_MASK) - 2048);
			unpackedSamples[0U] = short(int((pack >> 12) & FM_MASK) - 2048); // 

			// Process unpacked sample pair
			for (unsigned char j = 0U; j < 2U; j++) {
				// Convert to float (-1.0 to +1.0)
				float sampleFloat = (float(unpackedSamples[j]) * m_rxAudioGain) / 2048.0F;

				// De-emphasise and remove CTCSS
				if (m_deEmphasisOn)
					sampleFloat = m_deEmphasis->filter(sampleFloat);

				out[nOut++] = m_filterStage3->filter(m_filterStage2->filter(m_filterStage1->filter(sampleFloat)));
			}
		}

		return m_network->writeData(out, nOut);
	}

	return true;
}

unsigned int CFMControl::readModem(unsigned char* data, unsigned int space)
{
	assert(data != nullptr);
	assert(space > 0U);

	if (m_network == nullptr)
		return 0U;

	// A network end-of-transmission was received, reset the TX pre-emphasis
	// filter so that the next conversation starts with clean filter state.
	if (m_network->readEnd())
		m_preEmphasis->reset();

	if (space > 240U)		// 160 samples 12-bit
		space = 240U;		// 160 samples 12-bit

	float netData[160U]; // Modem can handle up to 160 samples at a time
	unsigned int length = m_network->readData(netData, 160U);  // 160 samples 12-bit
	if (length == 0U)
		return 0U;

	unsigned int pack = 0U;
	unsigned char* packPointer = (unsigned char*)&pack;
	unsigned int nData = 0U;

	for (unsigned int i = 0; i < length; i++) {
		float sampleFloat = netData[i] * m_txAudioGain;

		// Pre-emphasis
		if (m_preEmphasisOn)
			sampleFloat = m_preEmphasis->filter(sampleFloat);

		// Convert float to 12-bit samples (0 to 4095)
		unsigned int sample12bit = (unsigned int)((sampleFloat + 1.0F) * 2048.0F + 0.5F);

		// Pack 2 samples into 3 bytes
		if ((i & 1U) == 0U) {
			pack = 0U;
			pack = sample12bit << 12;
		} else {
			pack |= sample12bit;

			data[nData++] = packPointer[0U];
			data[nData++] = packPointer[1U];
			data[nData++] = packPointer[2U];
		}
	}

	return nData;
}

void CFMControl::clock(unsigned int ms)
{
	// May not be needed
}

void CFMControl::enable(bool enabled)
{
	// May not be needed
}

void CFMControl::writeJSON(const char* state)
{
	assert(state != nullptr);

	nlohmann::json json;

	json["timestamp"] = CUtils::createTimestamp();
	json["state"]     = state;

	WriteJSON("FM", json);
}

void CFMControl::writeJSONRSSI(int rssi)
{
	nlohmann::json json;

	json["timestamp"] = CUtils::createTimestamp();
	json["mode"]      = "FM";
	json["value"]     = rssi;

	WriteJSON("RSSI", json);
}
#endif
