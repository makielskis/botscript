status_train = {}
status_train["type"] = "mental"
status_train["type_from"] = "mental,physical"
status_train["timeslot"] = "0"
status_train["timeslot_from"] = "0,1,2,3,4,5,6"

interface_train = {}
interface_train["module"] = "Training"
interface_train["active"] = { input_type = "toggle", display_name = "Training starten" }
interface_train["type"] = { input_type = "dropdown", display_name = "Art" }
interface_train["timeslot"] = { input_type = "dropdown", display_name = "Dauer" }

function get_activity_time(page)
	-- Looting und Training-Restzeit auslesen
	local looting = util.get_by_regex(page, "#menu-looting .countdown .text',\\R([0-9]*),")
	local training = util.get_by_regex(page, "#menu-training .countdown .text',\\R([0-9]*),")

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
  local blocked = math.max(training, looting)
  util.log(blocked .. "s blocked")
	return blocked
end

function run_train()
  http.get_path("/freetime/training.html", function(page)
	  -- Aktivität prüfen
	  local activity_time = get_activity_time(page)
	  if activity_time ~= 0 then
      on_finish(activity_time, activity_time + 180)
      return
	  end

    util.log("starting training")
    local overlay_class = status_train["type"] .. "-overlay"
    local params = { timeSlot = status_train["timeslot"] }
    http.submit_form(page, "//div[contains(@class, '" .. overlay_class .. "')]//button[@name = 'start_training']", params, function(page)
      util.log("training started")
      local sleep_time = get_activity_time(page)
      on_finish(sleep_time, sleep_time + 180)
    end)
  end)
end
