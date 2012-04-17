/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 17. April 2012  makielskis@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "./module.h"

#include <string>

#include "boost/filesystem.hpp"

#include "./bot.h"

namespace botscript {

module::module(const std::string& script, bot* bot, lua_State* main_state) {
    using boost::filesystem::path;
    std::string filename = path(script).filename().generic_string();
    std::string module_name = filename.substr(0, filename.length() - 4);
    std::cout << module_name << "\n";
}

}  // namespace botscript
