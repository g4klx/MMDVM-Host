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

#include "Lights.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

const unsigned char LIGHTS_BUILT = 0xFFU;

static unsigned char word(const char* value, const char* const* words, unsigned int n)
{
	for (unsigned int i = 0U; i < n; i++) {
		if (::strcmp(value, words[i]) == 0)
			return (unsigned char)i;
	}

	return LIGHTS_BUILT;
}

static const char* const SERVICE[] = {"blink", "brief", "steady", "off"};
static const char* const IDLE[]    = {"cycle", "quiet"};
static const char* const PAIR[]    = {"own", "ysf-p25", "dstar-p25", "dstar-dmr"};

// "22:00" -> 1320; anything else -> -1.
static int minutes(const char* value)
{
	int h = 0, m = 0;
	if (::sscanf(value, "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59)
		return -1;

	return h * 60 + m;
}

CLights::CLights() :
m_off(false),
m_service(LIGHTS_BUILT),
m_invert(false),
m_idle(LIGHTS_BUILT),
m_call(true),
m_rxtx(true),
m_schedule(false),
m_from(22 * 60),
m_to(7 * 60),
m_zone()
{
	m_pair[0U] = m_pair[1U] = LIGHTS_BUILT;

	for (unsigned int i = 0U; i < 7U; i++)
		m_days[i] = true;
}

bool CLights::setKey(const char* key, const char* value)
{
	if (::strcmp(key, "LightsOff") == 0)
		m_off = ::atoi(value) == 1;
	else if (::strcmp(key, "ServiceLight") == 0)
		m_service = word(value, SERVICE, 4U);
	else if (::strcmp(key, "ServiceLightInvert") == 0)
		m_invert = ::atoi(value) == 1;
	else if (::strcmp(key, "IdleLights") == 0)
		m_idle = word(value, IDLE, 2U);
	else if (::strcmp(key, "CallLights") == 0)
		m_call = ::atoi(value) != 0;
	else if (::strcmp(key, "RXTXLights") == 0)
		m_rxtx = ::atoi(value) != 0;
	else if (::strcmp(key, "NXDNLights") == 0)
		m_pair[0U] = word(value, PAIR, 4U);
	else if (::strcmp(key, "POCSAGLights") == 0)
		m_pair[1U] = word(value, PAIR, 4U);
	else if (::strcmp(key, "LightsSchedule") == 0)
		m_schedule = ::atoi(value) == 1;
	else if (::strcmp(key, "LightsOffFrom") == 0) {
		int m = minutes(value);
		if (m >= 0)
			m_from = m;
	} else if (::strcmp(key, "LightsOffTo") == 0) {
		int m = minutes(value);
		if (m >= 0)
			m_to = m;
	} else if (::strcmp(key, "LightsOffDays") == 0) {
		for (unsigned int i = 0U; i < 7U; i++)
			m_days[i] = false;
		for (const char* p = value; *p != '\0'; p++) {
			if (*p >= '0' && *p <= '6')
				m_days[*p - '0'] = true;
		}
	} else if (::strcmp(key, "LightsZone") == 0)
		m_zone = value;
	else
		return false;

	return true;
}

void CLights::bytes(unsigned char* out, bool dark) const
{
	out[0U] = (m_off || dark) ? 1U : 0U;
	out[1U] = m_service;
	out[2U] = m_idle;
	out[3U] = m_call ? 1U : 0U;
	out[4U] = m_rxtx ? 1U : 0U;
	out[5U] = m_pair[0U];
	out[6U] = LIGHTS_BUILT;   // byte 6 is left as the build chose
	out[7U] = m_pair[1U];
	out[8U] = m_invert ? 1U : 0U;
}

bool CLights::scheduledDark(const struct tm& now) const
{
	if (!m_schedule)
		return false;

	int day       = now.tm_wday;
	int yesterday = (day + 6) % 7;
	int m         = now.tm_hour * 60 + now.tm_min;

	if (m_from == m_to)
		return m_days[day];
	if (m_from < m_to)
		return m_days[day] && m >= m_from && m < m_to;

	return (m_days[day] && m >= m_from) || (m_days[yesterday] && m < m_to);
}

std::string CLights::describe(bool dark) const
{
	if (m_off || dark)
		return dark && !m_off ? "all off (schedule)" : "all off";

	char buffer[160U];
	::snprintf(buffer, sizeof(buffer), "service %s%s, idle %s, call %s, RX/TX %s, NXDN %s, POCSAG %s",
		m_service == LIGHTS_BUILT ? "as built" : SERVICE[m_service], m_invert ? " (inverted)" : "",
		m_idle == LIGHTS_BUILT ? "as built" : IDLE[m_idle],
		m_call ? "on" : "off", m_rxtx ? "on" : "off",
		m_pair[0U] == LIGHTS_BUILT ? "as built" : PAIR[m_pair[0U]],
		m_pair[1U] == LIGHTS_BUILT ? "as built" : PAIR[m_pair[1U]]);

	return buffer;
}

const std::string& CLights::zone() const
{
	return m_zone;
}
