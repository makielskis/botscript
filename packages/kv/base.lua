function login(username, password)
	m_log("logging in")
	
	local param = {}
	param["username"] = username
	param["password"] = password
  local response = m_request_path("/")
	response = m_submit_form(response, "//form[starts-with(@action, '/login.html;jsessionid=')]", param)

	if string.find(response, '/logout.html', 0, true) then
		m_log("logged in")
		return true
	else
		m_log("not logged in")
		return false
	end
end

function length(t)
	local i = 0
	for k,v in pairs(t) do
		i = i + 1
	end
	return i
end

function concat_keys(t, delimiter)
	local s = ""
	local i = 0
	for k, v in pairs(t) do
		if i == 0 then
			s = v
			i = 1
		else
			s = s .. "," .. k
		end
	end
	return s
end

