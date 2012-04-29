function get_block_time(page)
	-- Cooldown und Missions-Restzeit auslesen
	local cooldown = m_get_by_regex(page, '.mission_cooldown_[0-9]*"\\), ([0-9]*),')
	local active = m_get_by_regex(page, "menu-missions .countdown .text',\\R([0-9]*),")

	-- cooldown Variable setzten
	if cooldown ~= "" then
		cooldown = tonumber(cooldown)
	else
		cooldown = 0
	end

	-- active Variable setzen
	if active ~= "" then
		active = tonumber(active)
	else
		active = 0
	end

	-- Maximum zurückgeben
	return math.max(active, cooldown)
end

function acknowledge_check(page)
	if m_get_by_xpath(page, "//button[@name = 'acknowledge']/@name") == "acknowledge" then
		m_log("acknowledging")
		m_submit_form(page, "//button[@name = 'acknowledge']")
		return true
	end
	return false
end

function run_mission()
	-- Missions Seite laden
	local page = m_request_path("/missions/standard.html")

	-- Acknowledge Check
	if acknowledge_check(page) then
		page = m_request_path("/missions/standard.html")
	end

	-- Cooldown / Missionsdauer überprüfen
	local block_time = get_block_time(page)
	if block_time ~= 0 then
		m_log(block_time .. "s blocked")
		return block_time, block_time + 180
	end

	-- Missions Link raussuchen
	local mission_link = m_get_by_xpath(page, "//a[contains(@href, 'view.html?actionTemplateId=56') \
						or contains(@href, 'view.html?actionTemplateId=36') \
						or contains(@href, 'view.html?actionTemplateId=4') \
						or contains(@href, 'view.html?actionTemplateId=34')]/@href")

	-- Abbruch, falls Link nicht gefunden
	if mission_link == "" then
		m_log_error("mission link not found")
		return 30, 60
	end

	-- Mission Start Seite aufrufen
	m_log("requesting mission start page")
	page = m_request_path(mission_link)

	-- Start URL finden
	local start_url = m_get_by_regex(page, 'startUrl\\:\'([^\']*)\'')

	-- Abbruch, falls Link nicht gefunden
	if (start_url == "") then
		m_log_error("mission start url not found")
		return 30, 60
	end

	-- Mission starten (POST request)
	m_log("starting mission")
	m_post_request("http://www.knastvoegel.de" .. start_url, "")

	return 0
end
