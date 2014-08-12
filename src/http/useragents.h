// Copyright (c) 2012, makielski.net
// Licensed under the MIT license
// https://raw.github.com/makielski/botscript/master/COPYING

#include <string>
#include <map>

#ifndef HTTP_USERAGENTS_H_
#define HTTP_USERAGENTS_H_

namespace http {

/// Class providing random useragent identification headers.
class useragents {
 public:
  enum { CHROME_OSX, CHROME_WIN7, NUMBER_OF_UAS };

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
    int id = static_cast<int>(std::floor(random * NUMBER_OF_UAS));

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
      case CHROME_OSX:
        h["Connection"] = "keep-alive";
        h["Cache-Control"] = "max-age=0";
        h["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8";
        h["User-Agent"] = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9_4) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/36.0.1985.125 Safari/537.36";
        h["Accept-Encoding"] = "gzip,deflate,sdch";
        h["Accept-Language"] = "en-US,en;q=0.8,de;q=0.6";
        break;

      case CHROME_WIN7:
        h["Connection"] = "keep-alive";
        h["Cache-Control"] = "max-age=0";
        h["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,/;q=0.8";
        h["User-Agent"] = "Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/36.0.1985.125 Safari/537.36";
        h["Accept-Encoding"] = "gzip,deflate,sdch";
        h["Accept-Language"] = "de-DE,de;q=0.8,en-US;q=0.6,en;q=0.4";
        break;
    }

    return h;
  }
};

}  // namespace http

#endif  // HTTP_USERAGENTS_H_
