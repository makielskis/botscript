/*
 * This code contains to one of the Makielski projects.
 * Visit http://makielski.net for more information.
 * 
 * Copyright (C) 11. April 2012  makielskis@gmail.com
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

#include <string>
#include <map>

#ifndef USERAGENTS
#define USERAGENTS

namespace http {

/// Class providing random useragent identification headers.
class useragents {
 public:
  enum { FF_WINXP, IE8_WIN7, OPERA_WINXP };

  /// Convenience alias for header key value map.
  typedef std::map<std::string, std::string> header;

  /**
   * Returns a random useragent from the useragents collection.
   *
   * \return random useragent headers
   */
  static header random_ua() {
    static unsigned int seed = 3552;
    seed *= 31;
    seed %= 32768;

    double random = static_cast<double>(seed) / 32768;
    int id = std::floor(random * ua_count);

    return ua(id);
  }

  /**
   * Returns the useragent headers from the useragent with the specified id.
   *
   * \param id the id of the useragent
   * \return the header of the specified useragent
   */
  static header ua(const int id) {
    header h;

    switch (id) {
      case FF_WINXP:
        h["User-Agent"] = "Mozilla/5.0 (Windows; U; Windows NT 5.1; de; rv:1.9.2.8) Gecko/20100722 Firefox/3.6.8";
        h["Accept"] = "text/html,image/*,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
        h["Accept-Language"] = "de-de,de;q=0.8,en-us;q=0.5,en;q=0.3";
        h["Accept-Encoding"] = "gzip,deflate";
        h["Accept-Charset"] = "ISO-8859-1,utf-8;q=0.7,*;q=0.7";
        h["Keep-Alive"] = "115";
        h["Connection"] = "keep-alive";
        h["Pragma"] = "no-cache";
        h["Cache-Control"] = "no-cache";
        break;

      case IE8_WIN7:
        h["Accept"] = "application/x-ms-application, image/jpeg, application/xaml+xml, image/gif, image/pjpeg, application/x-ms-xbap, */*";
        h["Accept-Language"] = "de-DE";
        h["User-Agent"] = "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; WOW64; Trident/4.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0)";
        h["Accept-Encoding"] = "gzip, deflate";
        h["Connection"] = "Keep-Alive";
        break;

      case OPERA_WINXP:
        h["User-Agent"] = "Opera/9.80 (Windows NT 5.1; U; de) Presto/2.6.30 Version/10.60";
        h["Accept"] = "text/html, image/*, application/xml;q=0.9, application/xhtml+xml, image/png, image/jpeg, image/gif, image/x-xbitmap, */*;q=0.1";
        h["Accept-Language"] = "de-DE,de;q=0.9,en;q=0.8";
        h["Accept-Charset"] = "iso-8859-1, utf-8, utf-16, *;q=0.1";
        h["Accept-Encoding"] = "deflate, gzip, x-gzip, identity, *;q=0";
        h["Cache-Control"] = "no-cache";
        h["Connection"] = "Keep-Alive, TE";
        h["TE"] = "deflate, gzip, chunked, identity, trailers";
        break;

      default:
        h["Accept"] = "image/gif, image/jpeg, image/pjpeg, image/pjpeg, application/x-shockwave-flash, application/xaml+xml, application/vnd.ms-xpsdocument, application/x-ms-xbap, application/x-ms-application, */*";
        h["Accept-Language"] = "de";
        h["User-Agent"] = "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 5.1; Trident/4.0; .NET CLR 2.0.50727; .NET CLR 3.0.04506.30; .NET CLR 3.0.4506.2152; .NET CLR 3.5.30729)";
        h["Accept-Encoding"] = "gzip, deflate";
        h["Connection"] = "Keep-Alive";
        break;
    }

    return h;
  }

 private:
  static const size_t ua_count = 3;
};

}  // namespace http

#endif  // USERAGENTS_H_
