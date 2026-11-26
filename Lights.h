/*
 *   Copyright (C) 2026 by Chip Cuccio W0CHP
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

#if !defined(Lights_H)
#define	Lights_H

#include <string>
#include <ctime>

// WPSD: how an MMDVM_HS modem's lights work, from [Modem], for firmware
// that says Lights (MMDVM_SET_LIGHTS). A key left out or empty keeps what
// the modem's own build chose.
//
//   LightsOff=0|1         every light dark
//   ServiceLight=         blink, brief (a short blink every 20 seconds or
//                         so), steady or off
//   ServiceLightInvert=0|1  the service light the other way round from
//                         the modem's build (a board wired to light it on
//                         the pin's other level)
//   IdleLights=           cycle (each mode's light as the modem listens
//                         for it) or quiet (dark until a call)
//   CallLights=1          the mode lights during a call
//   RXTXLights=1          the RX and TX lights
//   NXDNLights=           own, ysf-p25, dstar-p25 or dstar-dmr: which
//   POCSAGLights=         lights show that mode
//   LightsSchedule=0|1    every light dark between LightsOffFrom and
//   LightsOffFrom=22:00   LightsOffTo (local time) on LightsOffDays (0
//   LightsOffTo=07:00     Sunday ... 6 Saturday: the day a dark spell
//   LightsOffDays=0,1,2,3,4,5,6   starts; one past midnight ends the
//                         next morning). From = To: the whole day.
//   LightsZone=           the time zone the times are in (America/Chicago);
//                         empty: the system's
class CLights {
public:
	CLights();

	// A [Modem] key: true if it is one of the above.
	bool setKey(const char* key, const char* value);

	// MMDVM_SET_LIGHTS's nine bytes; dark puts every light out.
	void bytes(unsigned char* out, bool dark) const;

	// Whether the schedule has every light dark at now.
	bool scheduledDark(const struct tm& now) const;

	std::string describe(bool dark) const;

	const std::string& zone() const;

private:
	bool          m_off;
	unsigned char m_service;
	bool          m_invert;
	unsigned char m_idle;
	bool          m_call;
	bool          m_rxtx;
	unsigned char m_pair[2U];   // NXDN, POCSAG
	bool          m_schedule;
	int           m_from;   // minutes after midnight
	int           m_to;
	bool          m_days[7U];
	std::string   m_zone;
};

#endif
