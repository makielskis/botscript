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
  /// Convenience alias for header key value map.
  typedef std::map<std::string, std::string> header;

  enum ua_type { PG, KV };

  /**
   * \return a random int in [0 and max)
   */
  static int random_id(int max) {
    static unsigned int seed = 3552;
    seed *= 31;
    seed %= 32768;

    double random = static_cast<double>(seed) / 32768;
    return static_cast<int>(std::floor(random * max));
  }

  /**
   * Returns a random useragent from the useragents collection.
   *
   * \return random useragent headers
   */
  static header random_ua(ua_type type) {
    switch (type) {
      case ua_type::PG: return pg_ua();
      case ua_type::KV: return kv_ua();
      default: return header();
    }
  }

  /**
   * Returns the useragent headers from the useragent with the specified id.
   *
   * \param id the id of the useragent
   * \return the header of the specified useragent
   */
  static header kv_ua() {
    enum { CHROME_OSX, CHROME_WIN7, NUMBER_OF_UAS };

    header h;

    switch (random_id(NUMBER_OF_UAS)) {
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

  static header pg_ua() {
    enum { FF_WINXP, IE8_WIN7, OPERA_WINXP, NUMBER_OF_UAS };

    header h;

    switch (random_id(NUMBER_OF_UAS)) {
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
};

}  // namespace http

#endif  // HTTP_USERAGENTS_H_
