dofile('kv/base.lua')

status_looting = {}
status_looting["areas"] = ""
status_looting["slots"] = "0,1,2,3,4,5,6"
status_looting["caught_strategy"] = "bribe"
status_looting["gangtoll_strategy"] = "payToll"
status_looting["time_slot"] = ""
status_looting["area"] = ""

function get_territories(page)
	m_log("getting territories")

	-- Alle Area IDs suchen
	local matches = m_get_all_by_regex(page, "'([^']*)',\\R\\{id: ([0-9]*)")
	
	-- Ergebnis in eine Tabelle schreiben
	local result = {}
	for i1, match_table in ipairs(matches) do
		result[match_table[1]] = match_table[2]
	end

	-- GUI updaten
	if length(result) ~= 0 then
		m_set_status("areas", concat_keys(result, ","))
	end
	
	return result
end

function get_activity_time(page)
	-- Looting und Training-Restzeit auslesen
	local looting = m_get_by_regex(page, "#menu-looting .countdown .text',\\R([0-9]*),")
	local training = m_get_by_regex(page, "#menu-training .countdown .text',\\R([0-9]*),")

	-- looting Variable setzten
	if looting ~= "" then
		looting = tonumber(looting)
	else
		looting = 0
	end

	-- training Variable setzen
	if training ~= "" then
		training = tonumber(training)
	else
		training = 0
	end

	-- Maximum zurückgeben
	return math.max(training, looting)
end

function caught_check(page)
	m_log("caught check")
	if string.find(page, '/caught/gotcha.html', 0, true) then
		local strategy = status_looting['caught_strategy']
		local xpath = "//button[@type = 'submit' and @name = '" .. strategy .. "']"
		m_log("got caught - using strategy: " .. strategy)
		return m_submit_form(page, xpath)
	else
		m_log('writing debug')
		file = io.open('debug.html', 'w')
		file:write(page)
		file:close(file)
	end
	return page
end

function gangtoll_check(page)
	m_log("gang toll check")
	if string.find(page, 'looting.html?payToll=', 0, true) then
		local strategy = status_looting['gangtoll_strategy']
		m_log("gang toll - using strategy: " .. strategy)
		local link = m_get_by_xpath(page, "//a[contains(@href, '" .. strategy .. "')]/@href")
		if link == "" then
			m_log_error("gangtoll strategy link not found")
			return page
		end
		return m_request_path(link)
	end
	return page
end

function acknowledge_check(page)
	m_log("ack check")
	if string.find(page, '/freetime/looting.html?acknowledge=&amp;doContinue=true', 0, true) then
		m_log("acknowledging")
		return m_request_path('/freetime/looting.html?acknowledge=&doContinue=true')
	end
	return page
end

function run_looting()
	-- Seite aufrufen
	local page = m_request_path("/freetime/looting.html")

	-- Aktivität prüfen
	local activity_time = get_activity_time(page)
	if activity_time ~= 0 then
		m_log(activity_time .. "s blocked")
		return activity_time, activity_time + 180
	end
	
	-- 'Erwischt' und Bandensteuer Check
	page = caught_check(page)
	page = gangtoll_check(page)
	page = acknowledge_check(page)

	-- Mapping erstellen
	local id_mapping = get_territories(page)

	-- Eingabeparameter prüfen
	if status_looting["area"] == "" then
		m_log_error("area not set")
		return
	elseif status_looting["time_slot"] == "" then
		m_log_error("time_slot not set")
		return
	elseif status_looting["caught_strategy"] == "" then
		m_log_error("caught_strategy not set")
		return
	end

	-- Territory übersetzen
	local territory_id = id_mapping[status_looting["area"]]
	if territory_id == nil then
		-- Gebiet nicht gefunden - warte 1 bis 2min
		m_log_error("territory id not found - retrying")
		return 60, 120
	end
	
	-- Formular abschicken
	m_log("start looting")
	local xpath = "//button[@name = 'start_looting']"
  page = m_get_by_xpath(page, "//script[@id = 'loot_area-template']")
  local action = m_get_by_xpath(page, "//form[@class = 'startLootingForm']/@action")
	page = m_submit_form(page, xpath, {timeSlot = status_looting["time_slot"], territoryId = territory_id}, "http://www.knastvoegel.de" .. action)
	
	return 0
end
