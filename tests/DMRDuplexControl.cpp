// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the real slot decoder/state machine without opening a modem or socket.
#include "DMRSlot.h"
#include "DMRFullLC.h"
#include "DMRSlotType.h"
#include "DMREMB.h"
#include "DMRAccessControl.h"
#include "BPTC19696.h"
#include "CRC.h"
#include "Conf.h"
#include "Sync.h"
#include "Log.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <unistd.h>

bool g_listenOnly = false;

static std::vector<CDMRData> sent;
static unsigned int aborts = 0U;

// GNU ld wrappers intercept only the hardware/network boundary.
extern "C" bool __wrap__ZN11CDMRNetwork5writeERK8CDMRData(CDMRNetwork*, const CDMRData& data)
{
	sent.push_back(data);
	return true;
}
extern "C" bool __wrap__ZN6CModem13writeDMRAbortEj(CModem*, unsigned int)
{
	++aborts;
	return true;
}
extern "C" bool __wrap__ZN6CModem15writeDMRShortLCEPKhb(CModem*, const unsigned char*, bool)
{
	return true;
}

using Frame = std::array<unsigned char, DMR_FRAME_LENGTH_BYTES + 2U>;

static CDMRLC lc(unsigned int tg, FLCO flco = FLCO::GROUP, unsigned int src = 1234567U)
{
	CDMRLC value;
	value.setFLCO(flco);
	value.setSrcId(src);
	value.setDstId(tg);
	return value;
}

static Frame fullLC(const CDMRLC& value, unsigned char type)
{
	Frame frame{};
	frame[0U] = TAG_DATA;
	frame[1U] = DMR_SYNC_DATA | type;
	CDMRFullLC codec;
	codec.encode(value, frame.data() + 2U, type);
	CDMRSlotType slotType;
	slotType.setColorCode(1U);
	slotType.setDataType(type);
	slotType.getData(frame.data() + 2U);
	CSync::addDMRDataSync(frame.data() + 2U, false);
	return frame;
}


static Frame dataHeader(const CDMRLC& value)
{
	Frame frame{};
	frame[0U] = TAG_DATA;
	frame[1U] = DMR_SYNC_DATA | DT_DATA_HEADER;
	unsigned char data[12U] = {};
	data[0U] = 0x80U | DPF_UNCONFIRMED_DATA;
	data[2U] = value.getDstId() >> 16;
	data[3U] = value.getDstId() >> 8;
	data[4U] = value.getDstId();
	data[5U] = value.getSrcId() >> 16;
	data[6U] = value.getSrcId() >> 8;
	data[7U] = value.getSrcId();
	data[8U] = 1U;
	CCRC::addCCITT162(data, 12U);
	data[10U] ^= DATA_HEADER_CRC_MASK[0U];
	data[11U] ^= DATA_HEADER_CRC_MASK[1U];
	CBPTC19696 codec;
	codec.encode(data, frame.data() + 2U);
	CDMRSlotType slotType;
	slotType.setColorCode(1U);
	slotType.setDataType(DT_DATA_HEADER);
	slotType.getData(frame.data() + 2U);
	CSync::addDMRDataSync(frame.data() + 2U, false);
	return frame;
}

static Frame voice(CDMREmbeddedData& embedded, unsigned char n)
{
	Frame frame{};
	::memcpy(frame.data(), DMR_SILENCE_DATA, frame.size());
	frame[0U] = TAG_DATA;
	frame[1U] = n == 0U ? DMR_SYNC_AUDIO : n;
	if (n == 0U) {
		CSync::addDMRAudioSync(frame.data() + 2U, false);
	} else {
		CDMREMB emb;
		emb.setColorCode(1U);
		emb.setPI(false);
		emb.setLCSS(embedded.getData(frame.data() + 2U, n));
		emb.getData(frame.data() + 2U);
	}
	return frame;
}

static void rf(CDMRSlot& slot, Frame frame)
{
	slot.writeModem(frame.data(), frame.size());
}

static void network(CDMRSlot& slot, unsigned int slotNo, unsigned char type)
{
	const auto value = lc(91U, FLCO::GROUP, 2345678U);
	const auto frame = type == DT_DATA_HEADER ? dataHeader(value) : fullLC(value, type);
	CDMRData data;
	data.setSlotNo(slotNo);
	data.setDataType(type);
	data.setFLCO(value.getFLCO());
	data.setSrcId(value.getSrcId());
	data.setDstId(value.getDstId());
	data.setData(frame.data() + 2U);
	slot.writeNetwork(data);
}

static unsigned int drain(CDMRSlot& slot)
{
	Frame frame{};
	unsigned int count = 0U;
	while (slot.readModem(frame.data()) != 0U)
		++count;
	return count;
}

static void defaults()
{
	CDMRAccessControl::init({}, {}, {}, {}, false, {}, 1234567U);
	sent.clear();
	aborts = 0U;
}

static void checkConfig()
{
	char path[] = "/tmp/mmdvm-duplex-test-XXXXXX";
	const int fd = ::mkstemp(path);
	assert(fd >= 0);
	::close(fd);
	for (const auto& value : {"", "4000", "16777215", "16777216", "-1", "4000bad"}) {
		{
			std::ofstream file(path);
			file << "[DMR]\n";
			if (*value != '\0')
				file << "DuplexControlTG=" << value << "\n";
		}
		CConf conf(path);
		assert(conf.read());
		const unsigned int expected = std::string(value) == "4000" ? 4000U :
			(std::string(value) == "16777215" ? 16777215U : 0U);
		assert(conf.getDMRDuplexControlTG() == expected);
	}
	::unlink(path);
}

int main()
{
	LogInitialise(0U, 0U);
	CModem modem(true, false, false, false, 0U, 0U, false, false, false);
	CDMRNetwork net("127.0.0.1", 62031U, "127.0.0.1", 0U, 1234567U, true, true, true, false);
	CDMRLookup lookup("", 0U);
	CRSSIInterpolator rssi;
	const auto configure = [&](bool duplex, unsigned int tg) {
		CDMRSlot::init(1U, false, false, 0U, &modem, &net, duplex, &lookup,
			&rssi, 0U, DMR_OVCM::OFF, false, tg);
		defaults();
	};
	checkConfig();

	// Header, voice, terminator and slot number; keep queued downlink intact.
	for (unsigned int slotNo : {1U, 2U}) {
		configure(true, 4000U);
		CDMRSlot slot(slotNo, 180U);
		network(slot, slotNo, DT_VOICE_LC_HEADER);
		const auto value = lc(4000U);
		rf(slot, fullLC(value, DT_VOICE_LC_HEADER));
		assert(aborts == 1U); // only the network call's initial modem abort
		assert(drain(slot) == 3U); // all three downlink headers survive
		CDMREmbeddedData embedded;
		embedded.setLC(value);
		for (unsigned char n = 0U; n < 6U; ++n)
			rf(slot, voice(embedded, n));
		rf(slot, fullLC(value, DT_TERMINATOR_WITH_LC));
		assert(sent.size() == 8U);
		for (unsigned int i = 0U; i < sent.size(); ++i) {
			assert(sent[i].getSlotNo() == slotNo);
			assert(sent[i].getDstId() == 4000U);
			assert(sent[i].getSrcId() == 1234567U);
			assert(sent[i].getFLCO() == FLCO::GROUP);
			assert(sent[i].getSeqNo() == i);
		}
		assert(sent.front().getDataType() == DT_VOICE_LC_HEADER);
		assert(sent.back().getDataType() == DT_TERMINATOR_WITH_LC);
		assert(drain(slot) == 0U);
		slot.enable(false);
	}

	// The default, simplex, another TG and private calls retain the busy guard.
	for (unsigned int test = 0U; test < 4U; ++test) {
		configure(test != 1U, test == 0U ? 0U : 4000U);
		CDMRSlot slot(1U, 180U);
		network(slot, 1U, DT_VOICE_LC_HEADER);
		const auto value = lc(test == 2U ? 92U : 4000U, test == 3U ? FLCO::USER_USER : FLCO::GROUP);
		rf(slot, fullLC(value, DT_VOICE_LC_HEADER));
		rf(slot, fullLC(value, DT_TERMINATOR_WITH_LC));
		assert(sent.empty());
		slot.enable(false);
	}

	// Source and per-slot TG access checks still apply.
	for (bool rejectSource : {false, true}) {
		configure(true, 4000U);
		if (rejectSource)
			CDMRAccessControl::init({1234567U}, {}, {}, {}, false, {}, 1234567U);
		else
			CDMRAccessControl::init({}, {}, {91U}, {}, false, {}, 1234567U);
		CDMRSlot slot(1U, 180U);
		network(slot, 1U, DT_VOICE_LC_HEADER);
		rf(slot, fullLC(lc(4000U), DT_VOICE_LC_HEADER));
		assert(sent.empty());
		slot.enable(false);
	}

	// Finish the downlink before RF ends: no partial control call is repeated.
	configure(true, 4000U);
	{
		CDMRSlot slot(1U, 180U);
		network(slot, 1U, DT_VOICE_LC_HEADER);
		rf(slot, fullLC(lc(4000U), DT_VOICE_LC_HEADER));
		network(slot, 1U, DT_TERMINATOR_WITH_LC);
		drain(slot);
		CDMREmbeddedData embedded;
		embedded.setLC(lc(4000U));
		rf(slot, voice(embedded, 0U));
		rf(slot, fullLC(lc(4000U), DT_TERMINATOR_WITH_LC));
		assert(sent.size() == 3U);
		assert(drain(slot) == 0U);
		// A subsequent idle RF call follows the normal repeat path.
		sent.clear();
		rf(slot, fullLC(lc(92U), DT_VOICE_LC_HEADER));
		assert(sent.size() == 1U);
		assert(drain(slot) == 3U);
		slot.enable(false);
	}

	// Missing header: admit the call only after decoding its embedded LC.
	configure(true, 4000U);
	{
		CDMRSlot slot(2U, 180U);
		network(slot, 2U, DT_VOICE_LC_HEADER);
		CDMREmbeddedData embedded;
		embedded.setLC(lc(4000U));
		for (unsigned char n = 0U; n < 6U; ++n)
			rf(slot, voice(embedded, n));
		assert(!sent.empty());
		assert(sent.front().getDataType() == DT_VOICE_LC_HEADER);
		assert(sent.front().getDstId() == 4000U);
		assert(aborts == 1U);
		rf(slot, fullLC(lc(4000U), DT_TERMINATOR_WITH_LC));
		assert(sent.back().getDataType() == DT_TERMINATOR_WITH_LC);
		slot.enable(false);
		// Disable/re-enable must clear the network-only latch.
		slot.enable(true);
		sent.clear();
		rf(slot, fullLC(lc(92U), DT_VOICE_LC_HEADER));
		assert(sent.size() == 1U);
		assert(drain(slot) == 3U);
		slot.enable(false);
	}

	// Network data is not preempted, and RF data does not gain the exception.
	configure(true, 4000U);
	{
		CDMRSlot slot(1U, 180U);
		network(slot, 1U, DT_DATA_HEADER);
		rf(slot, fullLC(lc(4000U), DT_VOICE_LC_HEADER));
		rf(slot, fullLC(lc(4000U), DT_TERMINATOR_WITH_LC));
		assert(sent.empty());
		slot.enable(false);
	}
	configure(true, 4000U);
	{
		CDMRSlot slot(1U, 180U);
		network(slot, 1U, DT_VOICE_LC_HEADER);
		rf(slot, dataHeader(lc(4000U)));
		assert(sent.empty());
		slot.enable(false);
	}

	// A control call on slot 1 must not suppress normal idle repeating on slot 2.
	configure(true, 4000U);
	{
		CDMRSlot first(1U, 180U), second(2U, 180U);
		network(first, 1U, DT_VOICE_LC_HEADER);
		rf(first, fullLC(lc(4000U), DT_VOICE_LC_HEADER));
		rf(second, fullLC(lc(4000U), DT_VOICE_LC_HEADER));
		assert(sent.size() == 2U);
		assert(sent[0U].getSlotNo() == 1U && sent[1U].getSlotNo() == 2U);
		assert(drain(second) == 3U);
		first.enable(false);
		second.enable(false);
	}

	// Loss of the RF call clears the latch for the next idle call.
	configure(true, 4000U);
	{
		CDMRSlot slot(1U, 180U);
		network(slot, 1U, DT_VOICE_LC_HEADER);
		rf(slot, fullLC(lc(4000U), DT_VOICE_LC_HEADER));
		Frame lost{};
		lost[0U] = TAG_LOST;
		rf(slot, lost);
		network(slot, 1U, DT_TERMINATOR_WITH_LC);
		drain(slot);
		rf(slot, fullLC(lc(92U), DT_VOICE_LC_HEADER));
		assert(sent.back().getDstId() == 92U);
		assert(drain(slot) == 3U);
		slot.enable(false);
	}

	std::puts("DMR duplex control regression tests passed");
}
