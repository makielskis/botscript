function login(username, password)
  util.log("getting start page")
	http.get_path("/", function(response)
    if string.find(response, '/logout.html', 0, true) then
	    util.log("already logged in")
      on_finish(true)
      return
    end

    local param = {}
    param["username"] = username
    param["password"] = password

    util.log("logging in")
    http.submit_form(response, "//form[starts-with(@action, '/login.html;jsessionid=')]", param, function(response)
      util.log("checking response")
      if string.find(response, '/logout.html', 0, true) then
        util.log("logged in")
        on_finish(true)
        return
      else
        util.log("not logged in")
        on_finish(false)
        return
      end
    end)
  end)
end
