interface_collect = {}
interface_collect["module"] = "Flaschen sammeln"
interface_collect["active"] = { input_type = "toggle", display_name = "Sammeln gehen" }

function get_collect_time(page)
	link = m_get_by_xpath(page, "//a[@href = '/activities/' and @class= 'ttip']")
	seconds = m_get_by_regex(link, "counter\\((-?[0-9]*)\\)")
	return tonumber(seconds)
end

function get_pennerbar_info(key)
	local page = m_request_path("/pennerbar.xml")
	return tonumber(m_get_by_xpath(page, "//" .. key .. "/@value"))
end

function run_collect()
	m_log("starting to collect bottles")
	
	-- check for activity
	local page = m_request_path("/activities/")
	local collect_time = get_collect_time(page)
	local fight_time = get_pennerbar_info("kampftimer")
	if (collect_time > 0) then
		m_log("already collecting " .. collect_time)
		wait_time = collect_time
	elseif (fight_time > 0) then
		m_log("fighting" .. fight_time)
		wait_time = fight_time
	else
		-- empty cart
		if (m_get_by_xpath(page, "//input[@name = 'bottlecollect_pending']/@value") == "True") then
			m_log("clearing cart")
			m_submit_form(page, "//form[contains(@action, 'bottle')]")
		end
		
		-- check preset collect time
		local page = m_request_path("/activities/")
		local selected = m_get_by_xpath(page, "//select[@name = 'time']/option[@selected = 'selected']/@value")
		local parameters = {}
		parameters["sammeln"] = selected
		m_log("collect time " .. selected)
		
		-- start collection
		page = m_submit_form(page, "//form[contains(@name, 'starten')]", parameters, "/activities/bottle/")
		
		-- get collect time and sleep
		collect_time = get_collect_time(page)
		wait_time = collect_time
	end

	return wait_time, wait_time + 180
end
