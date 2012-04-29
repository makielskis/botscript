function login(username, password)
	m_log("logging in")

	local param = {}
	param["username"] = username
	param["password"] = password
  local response = m_request_path("/")
	response = m_submit_form(response, "//input[@name = 'submitForm']", param)

	if string.find(response, 'action="/logout/"', 0, true) then
		m_log("logged in")
		return true
	else
		m_log_error("not logged in")
		return false
	end
end
