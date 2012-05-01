status_sell = {}
status_sell["price"] = "-1"
status_sell["continuous"] = "0"
status_sell["amount"] = "0"

interface_sell = {}
interface_sell["module"] = "Flaschen verkaufen"
interface_sell["active"] = { input_type = "toggle", display_name = "Flaschen verkaufen" }
interface_sell["price"] = { input_type = "textfield", display_name = "Verkaufspreis" }
interface_sell["continuous"] = { input_type = "checkbox", display_name = "Dauerverkaufsmodus" }
interface_sell["amount"] = { input_type = "textfield", display_name = "Verkaufsmenge" }

function get_bottle_price(str)
	price = m_get_by_xpath(str, "//font[@id = 'wirkung']")
	
	a, b = string.find(price, ".%d%d")
	if a then
		return tonumber(string.sub(price, a + 1, b))
	end
	
	a, b = string.find(price, ",%d%d")
	if a then
		return tonumber(string.sub(price, a + 1, b))
	end
	
	
	return tonumber(price)
end

function get_max_bottles(str)
	max_bottles = m_get_by_regex(str, 'name="max"[^0-9]*([0-9]*)')
	return tonumber(max_bottles)
end

function run_sell()
	if tonumber(status_sell["price"]) <= 0 or tonumber(status_sell["amount"]) <= 0 and status_sell["continuous"] == "0" then
		-- Missing sell information - exit
		m_log_error("missing sell information")
		return -1
	else
		-- Read current bottle price from /stock/bottle/
		page = m_request_path("/stock/bottle/")
		current_price = get_bottle_price(page)

		-- Check if bottle price is greater or equal the sell price
		if current_price >= tonumber(status_sell["price"]) then
			parameters = {}
			if status_sell["continuous"] == "1" then
				-- Continous mode - sell all bottles (if any)
				parameters["sum"] = get_max_bottles(page)
				if tonumber(parameters["sum"]) ~= 0 then
					m_log("selling " .. parameters["sum"] .. " (all) bottles at " .. current_price)
				end
			else
				-- Non continuous mode - sell sell amount
				parameters["sum"] = status_sell["amount"]
				m_log("selling " .. status_sell["amount"] .. " bottles at " .. current_price)
			end

			-- Submit sell form
			m_submit_form(page, "//form[contains(@action, '/bottle/sell/')]", parameters)

			if status_sell["continuous"] == "1" then
				-- Wait 30sec - 3min until next check
				return 30, 180
			else
				-- Stop selling on non continous mode
				return -1
			end
		end
	end
end
